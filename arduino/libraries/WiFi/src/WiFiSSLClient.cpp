/*
  This file is part of the Arduino NINA firmware.
  Copyright (c) 2018 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include "esp_partition.h"

#include "WiFiSSLClient.h"

extern "C" {
  #include "esp_log.h"
}

class __Guard {
public:
  __Guard(SemaphoreHandle_t handle) {
    _handle = handle;

    xSemaphoreTakeRecursive(_handle, portMAX_DELAY);
  }

  ~__Guard() {
    xSemaphoreGiveRecursive(_handle);
  }

private:
  SemaphoreHandle_t _handle;
};

#define synchronized __Guard __guard(_mbedMutex);

WiFiSSLClient::WiFiSSLClient() :
  _connected(false),
  _peek(-1)
{
  _netContext.fd = -1;

  _mbedMutex = xSemaphoreCreateRecursiveMutex();
}

static int net_connect( mbedtls_net_context *ctx, const char *host, const char *port, int proto, uint16_t timeout);

int WiFiSSLClient::connect(const char* host, uint16_t port, bool sni)
{
  synchronized {
    _netContext.fd = -1;
    _connected = false;

    mbedtls_ssl_init(&_sslContext);
    mbedtls_ctr_drbg_init(&_ctrDrbgContext);
    mbedtls_ssl_config_init(&_sslConfig);
    mbedtls_entropy_init(&_entropyContext);
    mbedtls_x509_crt_init(&_caCrt);
    mbedtls_net_init(&_netContext);

    if (mbedtls_ctr_drbg_seed(&_ctrDrbgContext, mbedtls_entropy_func, &_entropyContext, NULL, 0) != 0) {
      stop();
      return 0;
    }

    if (mbedtls_ssl_config_defaults(&_sslConfig, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
      stop();
      return 0;
    }

    mbedtls_ssl_conf_authmode(&_sslConfig, MBEDTLS_SSL_VERIFY_REQUIRED);

    spi_flash_mmap_handle_t handle;
    const unsigned char* certs_data = {};

    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "certs");
    if (part == NULL)
    {
      return 0;
    }

    int ret = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, (const void**)&certs_data, &handle);
    if (ret != ESP_OK)
    {
      return 0;
    }

    ret = mbedtls_x509_crt_parse(&_caCrt, certs_data, strlen((char*)certs_data) + 1);
    if (ret < 0) {
      stop();
      return 0;
    }

    mbedtls_ssl_conf_ca_chain(&_sslConfig, &_caCrt, NULL);

    mbedtls_ssl_conf_rng(&_sslConfig, mbedtls_ctr_drbg_random, &_ctrDrbgContext);

    if (mbedtls_ssl_setup(&_sslContext, &_sslConfig) != 0) {
      stop();
      return 0;
    }

    if (sni && mbedtls_ssl_set_hostname(&_sslContext, host) != 0) {
      stop();
      return 0;
    }

    char portStr[6];
    itoa(port, portStr, 10);

    if (_connTimeout ? net_connect(&_netContext, host, portStr, MBEDTLS_NET_PROTO_TCP, _connTimeout)
        : mbedtls_net_connect(&_netContext, host, portStr, MBEDTLS_NET_PROTO_TCP)) {
      stop();
      return 0;
    }

    mbedtls_ssl_set_bio(&_sslContext, &_netContext, mbedtls_net_send, mbedtls_net_recv, NULL);

    int result;

    do {
      result = mbedtls_ssl_handshake(&_sslContext);
    } while (result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);

    if (result != 0) {
      stop();
      return 0;
    }

    mbedtls_net_set_nonblock(&_netContext);
    _connected = true;

    return 1;
  }
}

int WiFiSSLClient::connect(const char* host, uint16_t port)
{
  return connect(host, port, true);
}

int WiFiSSLClient::connect(/*IPAddress*/uint32_t ip, uint16_t port)
{
  char ipStr[16];

  sprintf(ipStr, "%d.%d.%d.%d", ((ip & 0xff000000) >> 24), ((ip & 0x00ff0000) >> 16), ((ip & 0x0000ff00) >> 8), ((ip & 0x000000ff) >> 0)/*ip[0], ip[1], ip[2], ip[3]*/);

  return connect(ipStr, port, false);
}

size_t WiFiSSLClient::write(uint8_t b)
{
  return write(&b, 1);
}

size_t WiFiSSLClient::write(const uint8_t *buf, size_t size)
{
  synchronized {
    int written = mbedtls_ssl_write(&_sslContext, buf, size);

    if (written < 0) {
      written = 0;
    }

    return written;
  }
}

int WiFiSSLClient::available()
{
  synchronized {
    int result = mbedtls_ssl_read(&_sslContext, NULL, 0);

    int n = mbedtls_ssl_get_bytes_avail(&_sslContext);

    if (n == 0 && result != 0 && result != MBEDTLS_ERR_SSL_WANT_READ) {
      stop();
    }

    return n;
  }
}

int WiFiSSLClient::read()
{
  uint8_t b;

  if (_peek != -1) {
    b = _peek;
    _peek = -1;
  } else if (read(&b, sizeof(b)) == -1) {
    return -1;
  }

  return b;
}

int WiFiSSLClient::read(uint8_t* buf, size_t size)
{
  synchronized {
    if (!available()) {
      return -1;
    }

    int result = mbedtls_ssl_read(&_sslContext, buf, size);

    if (result < 0) {
      if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) {
        stop();
      }

      return -1;
    }

    return result;
  }
}

int WiFiSSLClient::peek()
{
  if (_peek == -1) {
    _peek = read();
  }

  return _peek;
}

void WiFiSSLClient::flush()
{
}

void WiFiSSLClient::stop()
{
  synchronized {
    if (_netContext.fd > 0) {
      mbedtls_ssl_session_reset(&_sslContext);    

      mbedtls_net_free(&_netContext);
      mbedtls_x509_crt_free(&_caCrt);
      mbedtls_entropy_free(&_entropyContext);
      mbedtls_ssl_config_free(&_sslConfig);
      mbedtls_ctr_drbg_free(&_ctrDrbgContext);
      mbedtls_ssl_free(&_sslContext);
    }

    _connected = false;
    _netContext.fd = -1;
  }

  vTaskDelay(1);
}

uint8_t WiFiSSLClient::connected()
{
  synchronized {
    if (!_connected) {
      return 0;
    }

    if (available()) {
      return 1;
    }

    return 1;
  }
}

WiFiSSLClient::operator bool()
{
  return ((_netContext.fd != -1) && _connected);
}

/*IPAddress*/uint32_t WiFiSSLClient::remoteIP()
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  getpeername(_netContext.fd, (struct sockaddr*)&addr, &len);

  return ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
}

uint16_t WiFiSSLClient::remotePort()
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  getpeername(_netContext.fd, (struct sockaddr*)&addr, &len);

  return ntohs(((struct sockaddr_in *)&addr)->sin_port);
}


/*
 * based on mbedtls_net_connect, but with timeout support
 */
int net_connect(mbedtls_net_context *ctx, const char *host, const char *port, int proto, uint16_t timeout) {
  int ret;
  struct addrinfo hints, *addr_list, *cur;

  /* Do name resolution with both IPv6 and IPv4 */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
  hints.ai_protocol =
      proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;

  if ( getaddrinfo( host, port, &hints, &addr_list ) != 0) {
    return ( MBEDTLS_ERR_NET_UNKNOWN_HOST);
  }

  /* Try the sockaddrs until a connection succeeds */
  ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
  for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
    int fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);

    if (fd < 0) {
      ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
      continue;
    }

    mbedtls_net_context tmpCtx;
    tmpCtx.fd = fd;
    mbedtls_net_set_nonblock(&tmpCtx);

    int res = connect(fd,  cur->ai_addr, cur->ai_addrlen);
    if (res < 0 && errno != EINPROGRESS) {
      ESP_LOGW("WiFiSSLClient", "connect on fd %d, errno: %d, \"%s\"", fd, errno, strerror(errno));
    } else {
      struct timeval tv;
      tv.tv_sec = timeout / 1000;
      tv.tv_usec = (timeout % 1000) * 1000;

      fd_set fdset;
      FD_ZERO(&fdset);
      FD_SET(fd, &fdset);

      res = select(fd + 1, nullptr, &fdset, nullptr, &tv);
      if (res < 0) {
        ESP_LOGW("WiFiSSLClient", "select on fd %d, errno: %d, \"%s\"", fd, errno, strerror(errno));
      } else if (res == 0) {
        ESP_LOGW("WiFiSSLClient", "select returned due to timeout %d ms for fd %d", timeout, fd);
      } else {
        int sockerr;
        socklen_t len = (socklen_t) sizeof(int);
        res = getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &len);
        if (res < 0) {
          ESP_LOGW("WiFiSSLClient", "getsockopt on fd %d, errno: %d, \"%s\"", fd, errno, strerror(errno));
        } else if (sockerr != 0) {
          ESP_LOGW("WiFiSSLClient", "socket error on fd %d, errno: %d, \"%s\"", fd, sockerr, strerror(sockerr));
        } else {
          ctx->fd = fd; // connected!
          ret = 0;
          mbedtls_net_set_block(ctx); // back to blocking for SSL handshake
          break;
        }
      }
    }
    close(fd);
    ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
  }

  freeaddrinfo(addr_list);

  return (ret);
}

