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

#include "Arduino.h"
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include "esp_partition.h"

#include "WiFiSSLClient.h"

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

int WiFiSSLClient::connect(const char* host, uint16_t port)
{
  ets_printf("** Connect Called");
  // Hardcode CERT
  const char AWS_CERT_CRT[] = "-----BEGIN CERTIFICATE-----\n" \
"MIIDWTCCAkGgAwIBAgIUHi7YIHwvdKnUKTKE4MzqaVvVW7QwDQYJKoZIhvcNAQEL\n" \
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n" \
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTE5MDkyNTE2NDA1\n" \
"NVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n" \
"ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMygEW9cO1ZXQY4Fo3PY\n" \
"vBGV6WHwJYKIOd5iTZ4MQmkYNqn9q2YnuXEwYJ+sw6QxSYyZ9O8yniZfviggJ2Dg\n" \
"GdTGKIbSK7B/C3w6cLnwPNsKbA2xsxnQU3yoQ99noaue4kG+WL7a5SHJHwzcFpT4\n" \
"tVffsUlFtI9fTyGg75+0X4OJiKtzPhpVrCDesKDl0wLewqqgfxasgXWk3bLGCcBy\n" \
"7YPEM2x0lp6644xz0jkJ/3KO09+AuFG54K+zv7UZOi4Tph8eiKnI2/2sM58yC233\n" \
"pCnB8gtxCegvJJ1ByM5SR3Zw5C1hq6cgN5ePv1fQ7QqOnIHygc0gDp8/nw5gnH8P\n" \
"3LcCAwEAAaNgMF4wHwYDVR0jBBgwFoAU1YI5dEJDKJgyKP6e/lSezmki1tUwHQYD\n" \
"VR0OBBYEFDTH23PCBu1Pw4xdOR3rY3Pcueh4MAwGA1UdEwEB/wQCMAAwDgYDVR0P\n" \
"AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQA1p78t3Tk+6V5h0SlokRaC5bVm\n" \
"RoXwXRmmCsZJlwvIG25buBdUAWC/2odreV4anM9HmRnECxZMIV7Q0NiuVcl3Kiok\n" \
"xtWsdsCyZkH0OMcBuiTEu+o3osTtxAp8dkzcBlh768htDXZCsAzRjFTwtZ78BqFk\n" \
"rzduv1FDtpbxoD95X8B3MOc+ZrsZ5TTA+dpepeid6K3jmG9LPmFnahCkK31Hp5dv\n" \
"WKKDKZn51PvOVAvti1QeAYcFabgeXFWb8OuCJcqWEKFJuvQRvKrpyLfpSR4NNq7M\n" \
"nM12jsbhjrGYVCmQjczqOMqF+LMnXYUSY+o6gsBCM5XRAwOLY4S7Gv53K4+l\n" \
"-----END CERTIFICATE-----\n";

  // hardcode private key
  const char AWS_CERT_PRIVATE[] =
"-----BEGIN RSA PRIVATE KEY-----\n" \
"MIIEowIBAAKCAQEAzKARb1w7VldBjgWjc9i8EZXpYfAlgog53mJNngxCaRg2qf2r\n" \
"Zie5cTBgn6zDpDFJjJn07zKeJl++KCAnYOAZ1MYohtIrsH8LfDpwufA82wpsDbGz\n" \
"GdBTfKhD32ehq57iQb5YvtrlIckfDNwWlPi1V9+xSUW0j19PIaDvn7Rfg4mIq3M+\n" \
"GlWsIN6woOXTAt7CqqB/FqyBdaTdssYJwHLtg8QzbHSWnrrjjHPSOQn/co7T34C4\n" \
"Ubngr7O/tRk6LhOmHx6Iqcjb/awznzILbfekKcHyC3EJ6C8knUHIzlJHdnDkLWGr\n" \
"pyA3l4+/V9DtCo6cgfKBzSAOnz+fDmCcfw/ctwIDAQABAoIBAGV+N2eevaezm8ZP\n" \
"saTyKUYnrxxuuowl+V3+MDVmK0JpSiPCuFLw/R/ROPu5+0fjUnG0ozJJTvwFnRHV\n" \
"8PIx9V3983f7osPmH9I8QlFXgTe70aBxNT5mgCJia1fR1PSE2AB34xi3BdNeKFJ+\n" \
"j4zQV1IAl7SaKFa8lUk+w9vY4U8h8kjj2OZWBsYKXz2Gk0WZP0mdP+eQeGtKVmWB\n" \
"HE+QO+fZIkT/QaZzntFYPfDy5accZRFelZt+opOVQvyi3riUeBlcRi5Xd+cPfKNY\n" \
"J0MQem2yBUuNiqCiaWAgIcFHHAI2kwB+4ju79aRFGlgP6ocFG/MazMV9GJWjGUGB\n" \
"1PoQ54ECgYEA/LSKbwpWVYVQhoxlJzet248Mj78dM3r4mYHmqrfxECycOu25pl0B\n" \
"TcU8g9t1ZxGdBwA4rKFRmYvIWSiwKRd36VsKHihFLxGAE7Vr0/DQgsBPlM88+Ca8\n" \
"fCEt6/NMr0U+hLZUMgTcG5L5P1cjC6DmM+zJHldkLAsHsDrKLwoqXiECgYEAz0sM\n" \
"xRc4ROs27POVqSl00FdBApFItYMR8O7FDYhWbX4Nghgd2WuQr4X7qbYsQZtRL8Ip\n" \
"InbeA/4KMG3Wxu/uMCyltNBhzx0ZfmF5pYv+BkmUPJY6quaQjagW72IgaIDwbg7I\n" \
"Rohu2L5tZqZ0ryjsokrbzOT31SM9UA1ijlBa79cCgYAbyiLbGTDrULDNSw0opefZ\n" \
"mD6SZDrq2WATSYS7S2UYGT/I/zGGSP4GtmT0PyMHBZnWFkElQsw9bXDH1UCiFDGc\n" \
"mOVg8Z7CEVObVz0XXokfh9R4kd2rkF7z65YoN2Y8dAnvADn8Eiq+YYhFXei9s6D9\n" \
"HtHzIzsh2MisqZpoV97W4QKBgAKaZ6ul8f/zkDoDiRKZwazIG7njhy04WyZSaUkV\n" \
"ODihx5uln+JWFngNz64+6mlcgPV/k7KqGXmlXA1lo7fV1YDnXqFZqJDIRcSvhq6M\n" \
"hoEftWvZWx1ATfppbPhOnCeTzvEi4GL6XaH9KjSKzJZShj43gHEfQvl7Os7hjCZL\n" \
"Xgj7AoGBALaXRmY0vHziaX8A0IpSklxiQm0cO4pxl2S9YQdHmzjEnLE3STJZkIBo\n" \
"YJwtTj3ZgQ7YDWFwzObX/UVi8Smnwf+qNnPqY9IpOKUKxUjFGCrGdiAITTCZfbMo\n" \
"8cD8F7nkbodpQNEXKEEWLTkMq0UQH813Fe2mltgrHPJ94YYIwfK6\n" \
"-----END RSA PRIVATE KEY-----\n";

  //_cert = AWS_CERT_CRT;
  _private_key = AWS_CERT_PRIVATE;

  return connect(host, port, _cert, _private_key);
}

int WiFiSSLClient::connect(const char* host, uint16_t port, const char* client_cert, const char* client_key)
{
  int ret, flags;
  synchronized {
    _netContext.fd = -1;
    _connected = false;

    ets_printf("*** connect init\n");
    // SSL Client Initialization
    mbedtls_ssl_init(&_sslContext);
    mbedtls_ctr_drbg_init(&_ctrDrbgContext);
    mbedtls_ssl_config_init(&_sslConfig);


    mbedtls_net_init(&_netContext);

    ets_printf("*** connect inited\n");

    ets_printf("*** connect drbgseed\n");
    mbedtls_entropy_init(&_entropyContext);
    // Seeds and sets up CTR_DRBG for future reseeds, pers is device personalization (esp)
    ret = mbedtls_ctr_drbg_seed(&_ctrDrbgContext, mbedtls_entropy_func,
                              &_entropyContext, (const unsigned char *) pers, strlen(pers));
    if (ret < 0) {
      ets_printf("Unable to set up mbedtls_entropy.\n");
      stop();
      return 0;
    }

    ets_printf("*** connect ssl hostname\n");
     /* Hostname set here should match CN in server certificate */
    if(mbedtls_ssl_set_hostname(&_sslContext, host) != 0) {
      stop();
      return 0;
    }

    ets_printf("*** connect ssl config\n");
     ret= mbedtls_ssl_config_defaults(&_sslConfig, MBEDTLS_SSL_IS_CLIENT,
                                        MBEDTLS_SSL_TRANSPORT_STREAM,
                                        MBEDTLS_SSL_PRESET_DEFAULT);
      if (ret != 0) {
      stop();
      ets_printf("Error Setting up SSL Config: %d", ret);
      return 0;
      }

    ets_printf("*** connect authmode\n");
    // we're always using the root CA cert from partition, so MBEDTLS_SSL_VERIFY_REQUIRED
    ets_printf("*** Loading CA Cert...\n");
    mbedtls_x509_crt_init(&_caCrt);
    mbedtls_ssl_conf_authmode(&_sslConfig, MBEDTLS_SSL_VERIFY_REQUIRED);

    // setting up CA certificates from partition
    spi_flash_mmap_handle_t handle;
    const unsigned char* certs_data = {};
    ets_printf("*** connect part findfirst\n");
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "certs");
    if (part == NULL) {
      stop();
      return 0;
    }

    ets_printf("*** connect part mmap\n");
    int ret = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, (const void**)&certs_data, &handle);
    if (ret != ESP_OK) {
      ets_printf("*** Error partition mmap %d\n", ret);
      stop();
      return 0;
    }

    ets_printf("*** connect crt parse\n");
    ret = mbedtls_x509_crt_parse(&_caCrt, certs_data, strlen((char*)certs_data) + 1);
    ets_printf("*** connect conf ca chain\n");
    mbedtls_ssl_conf_ca_chain(&_sslConfig, &_caCrt, NULL);
    if (ret < 0) {
      stop();
      return 0;
    }

    ets_printf("*** check for client_cert and client_key\n");
    if (client_cert != NULL && client_key != NULL) {
        mbedtls_x509_crt_init(&_clientCrt);
        mbedtls_pk_init(&_clientKey);

        ets_printf("Loading client certificate.");
        // note: +1 added for line ending
        ret = mbedtls_x509_crt_parse(&_clientCrt, (const unsigned char *)client_cert, strlen(client_cert) + 1);
        if (ret != 0) {
          ets_printf("Client cert not parsed, %d\n", ret);
          ets_printf("\nCert: \n %s", &_clientCrt);
          stop();
          return 0;
        }

        ets_printf("Loading private key.\n");
        ret = mbedtls_pk_parse_key(&_clientKey, (const unsigned char *)client_key, strlen(client_key)+1,
                                   NULL, 0);
        if (ret != 0) {
          ets_printf("Private key not parsed properly: %d\n", ret);
          stop();
          return 0;
        }
        // set own certificate chain and key
        ret = mbedtls_ssl_conf_own_cert(&_sslConfig, &_clientCrt, &_clientKey);
        if (ret != 0) {
          if (ret == -0x7f00) {
            ets_printf("Memory allocation failed, MBEDTLS_ERR_SSL_ALLOC_FAILED");
          }
          ets_printf("Private key not parsed properly: %d\n", ret);
          stop();
          return 0;
        }
    }
    else {
      ets_printf("Client certificate and key not provided.");
    }

    ets_printf("*** connect conf RNG\n");
    mbedtls_ssl_conf_rng(&_sslConfig, mbedtls_ctr_drbg_random, &_ctrDrbgContext);

    ets_printf("*** connect ssl setup\n");
    if ((ret = mbedtls_ssl_setup(&_sslContext, &_sslConfig)) != 0) {
      if (ret == -0x7f00){
        ets_printf("MBEDTLS_ERR_SSL_ALLOC_FAILED");
        ets_printf("Free internal heap after TLS %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));
      }
      ets_printf("Unable to connect ssl setup %d", ret);
      stop();
      return 0;
    }

    char portStr[6];
    itoa(port, portStr, 10);
    ets_printf("*** connect netconnect\n");
    if (mbedtls_net_connect(&_netContext, host, portStr, MBEDTLS_NET_PROTO_TCP) != 0) {
      stop();
      return 0;
    }

    ets_printf("*** connect set bio\n");
    mbedtls_ssl_set_bio(&_sslContext, &_netContext, mbedtls_net_send, mbedtls_net_recv, NULL);

    ets_printf("*** start SSL/TLS handshake...");
    unsigned long start_handshake = millis();
    // ref: https://tls.mbed.org/api/ssl_8h.html#a4a37e497cd08c896870a42b1b618186e
    while ((ret = mbedtls_ssl_handshake(&_sslContext)) !=0) {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        ets_printf("Error performing SSL handshake");
      }
      if((millis() - start_handshake) > handshake_timeout){
        ets_printf("Handshake timeout");
        stop();
        return -1;
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (client_cert != NULL && client_key != NULL)
    {
      ets_printf("Protocol is %s Ciphersuite is %s", mbedtls_ssl_get_version(&_sslContext), mbedtls_ssl_get_ciphersuite(&_sslContext));
    }
    ets_printf("Verifying peer X.509 certificate");
    char buf[512];
    if ((flags = mbedtls_ssl_get_verify_result(&_sslContext)) != 0) {
      bzero(buf, sizeof(buf));
      mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
      ets_printf("Failed to verify peer certificate! verification info: %s", buf);
      stop(); // invalid certificate, stop
      return -1;
    } else {
      ets_printf("Certificate chain verified.");
    }

    ets_printf("*** ssl set nonblock\n");
    mbedtls_net_set_nonblock(&_netContext);

    //ets_printf("Free internal heap before cleanup: %u\n", ESP.getFreeHeap());
    // free up the heap
    if (certs_data != NULL) {
      mbedtls_x509_crt_free(&_caCrt);
    }
    if (client_cert != NULL) {
      mbedtls_x509_crt_free(&_clientCrt);
    }
    if (client_key !=NULL) {
      mbedtls_pk_free(&_clientKey);
    }
    //ets_printf("Free internal heap after cleanup: %u\n", ESP.getFreeHeap());
    _connected = true;
    return 1;
  }
}

int WiFiSSLClient::connect(/*IPAddress*/uint32_t ip, uint16_t port)
{
  char ipStr[16];

  sprintf(ipStr, "%d.%d.%d.%d", ((ip & 0xff000000) >> 24), ((ip & 0x00ff0000) >> 16), ((ip & 0x0000ff00) >> 8), ((ip & 0x000000ff) >> 0)/*ip[0], ip[1], ip[2], ip[3]*/);

  return connect(ipStr, port);
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

void WiFiSSLClient::setCertificate(const char *client_ca)
{
  ets_printf("\n*** Setting client certificate...\n");
  _cert = client_ca;
  ets_printf("%s", client_ca);
  ets_printf("\n*** Set client certificate\n");
}

void WiFiSSLClient::setPrivateKey(const char *private_key)
{
  ets_printf("Setting client private key...\n");
  _private_key = private_key;
  ets_printf("Set client private key\n");
}

void WiFiSSLClient::setHandshakeTimeout(unsigned long timeout)
{
  handshake_timeout = timeout * 1000;
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
      mbedtls_x509_crt_free(&_clientCrt);
      mbedtls_pk_free(&_clientKey);
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
