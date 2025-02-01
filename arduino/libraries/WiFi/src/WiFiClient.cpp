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

#include <errno.h>
#include <string.h>

#include <lwip/sockets.h>

#include "WiFi.h"

#include "WiFiClient.h"

extern "C" {
  #include "esp_log.h"
}

WiFiClient::WiFiClient() :
  WiFiClient(-1)
{
}

WiFiClient::WiFiClient(int socket) :
  _socket(socket)
{
}

int WiFiClient::connect(const char* host, uint16_t port)
{
  uint32_t address;

  if (!WiFi.hostByName(host, address)) {
    return 0;
  }

  return connect(address, port);
}

int WiFiClient::connect(IPAddress ip, uint16_t port)
{
  _socket = lwip_socket(AF_INET, SOCK_STREAM, 0);

  if (_socket < 0) {
    _socket = -1;
    return 0;
  }

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (uint32_t)ip;
  addr.sin_port = htons(port);

  if (_connTimeout == 0) {
  if (lwip_connect_r(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    lwip_close_r(_socket);
    _socket = -1;
    return 0;
  }
  }

  int nonBlocking = 1;
  lwip_ioctl_r(_socket, FIONBIO, &nonBlocking);

  if (_connTimeout > 0) {
    int res = lwip_connect_r(_socket, (struct sockaddr*)&addr, sizeof(addr));
    if (res < 0 && errno != EINPROGRESS) {
      ESP_LOGW("WiFiClient", "connect on socket %d, errno: %d, \"%s\"", _socket, errno, strerror(errno));
      lwip_close_r(_socket);
      _socket = -1;
      return 0;
    }

    struct timeval tv;
    tv.tv_sec = _connTimeout / 1000;
    tv.tv_usec = (_connTimeout  % 1000) * 1000;
  
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(_socket, &fdset);

    res = select(_socket + 1, nullptr, &fdset, nullptr, &tv);
    if (res < 0) {
      ESP_LOGW("WiFiClient", "select on socket %d, errno: %d, \"%s\"", _socket, errno, strerror(errno));
      lwip_close_r(_socket);
      return 0;
    }
    if (res == 0) {
      ESP_LOGW("WiFiClient", "select returned due to timeout %d ms for socket %d", _connTimeout,  _socket);
      lwip_close_r(_socket);
      return 0;
    }
    int sockerr;
    socklen_t len = (socklen_t) sizeof(int);
    res = lwip_getsockopt(_socket, SOL_SOCKET, SO_ERROR, &sockerr, &len);
    if (res < 0) {
      ESP_LOGW("WiFiClient", "getsockopt on socket %d, errno: %d, \"%s\"", _socket, errno, strerror(errno));
      lwip_close_r(_socket);
      return 0;
    }
    if (sockerr != 0) {
      ESP_LOGW("WiFiClient", "socket error on socket %d, errno: %d, \"%s\"", _socket, sockerr, strerror(sockerr));
      lwip_close_r(_socket);
      return 0;
    }
  }
  return 1;
}

size_t WiFiClient::write(uint8_t b)
{
  return write(&b, 1);
}

size_t WiFiClient::write(const uint8_t *buf, size_t size)
{
  if (_socket == -1) {
    return 0;
  }

  int result = lwip_send_r(_socket, (void*)buf, size, MSG_PEEK);

  if (result < 0) {
    return 0;
  }

  return result;

}

int WiFiClient::available()
{
  if (_socket == -1) {
    return 0;
  }

  int result = 0;

  //This function returns the number of bytes of pending data already received in the socket’s network.
  if (lwip_ioctl_r(_socket, FIONREAD, &result) < 0) {
    lwip_close_r(_socket);
    _socket = -1;
    return 0;
  }

  return result;
}

int WiFiClient::read()
{
  uint8_t b;

  if (read(&b, sizeof(b)) == -1) {
    return -1;
  }

  return b;
}

int WiFiClient::read(uint8_t* buf, size_t size)
{
  if (!available()) {
    return -1;
  }

  int result = lwip_recv_r(_socket, buf, size, MSG_DONTWAIT);

  if (result <= 0 && errno != EWOULDBLOCK) {
    lwip_close_r(_socket);
    _socket = -1;
    return 0;
  }

  if (result == 0) {
    result = -1;
  }

  return result;
}

int WiFiClient::peek()
{
  uint8_t b;

  //This function tries to receive data from the network and can return an error if the connection when down.
  if (lwip_recv_r(_socket, &b, sizeof(b), MSG_PEEK | MSG_DONTWAIT) <= 0) {
    if (errno != EWOULDBLOCK) {
      lwip_close_r(_socket);
      _socket = -1;
    }

    return -1;
  }

  return b;
}

void WiFiClient::flush()
{
}

void WiFiClient::stop()
{
  if (_socket != -1) {
    lwip_close_r(_socket);
    _socket = -1;
  }
}

uint8_t WiFiClient::connected()
{
  if (_socket != -1) {
    //Check if there are already available data and, if not, try to read new ones from the network.
    if (!available()) {
      peek();
    }
  }

  return (_socket != -1);
}

WiFiClient::operator bool()
{
  return (_socket != -1);
}

bool WiFiClient::operator==(const WiFiClient &other) const
{
  return (_socket == other._socket);
}

/*IPAddress*/uint32_t WiFiClient::remoteIP()
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  getpeername(_socket, (struct sockaddr*)&addr, &len);

  return ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
}

uint16_t WiFiClient::remotePort()
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);

  getpeername(_socket, (struct sockaddr*)&addr, &len);

  return ntohs(((struct sockaddr_in *)&addr)->sin_port);
}
