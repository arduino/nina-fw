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

#include "WiFiClient.h"
#include "WiFiServer.h"

WiFiServer::WiFiServer() :
  WiFiServer(0)
{
}

WiFiServer::WiFiServer(uint16_t port) :
  _port(port),
  _socket(-1)
{
  for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
    _spawnedSockets[i] = -1;
  }
}

void WiFiServer::begin()
{
  _socket = lwip_socket(AF_INET, SOCK_STREAM, 0);

  if (_socket < 0) {
    return;
  }

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (uint32_t)0;
  addr.sin_port = htons(_port);

  if (lwip_bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    lwip_close_r(_socket);
    _socket = -1;
    return;
  }

  if (lwip_listen(_socket, 1) < 0) {
    lwip_close_r(_socket);
    _socket = -1;
    return;
  }

  int nonBlocking = 1;
  lwip_ioctl_r(_socket, FIONBIO, &nonBlocking);

  return;
}

WiFiClient WiFiServer::available(uint8_t* status)
{
  int result = lwip_accept(_socket, NULL, 0);

  if (status) {
    *status = (result != -1);
  }

  if (result != -1) {
    // store the connected socket
    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
      if (_spawnedSockets[i] == -1) {
        _spawnedSockets[i] = result;
        break;
      }
    }
  }

  result = -1;

  // find an existing socket with data
  for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
    if (_spawnedSockets[i] != -1) {
      WiFiClient c(_spawnedSockets[i]);

      if (!c.connected()) {
        // socket not connected, clear from book keeping
        _spawnedSockets[i] = -1;
      } else if (c.available()) {
        result = _spawnedSockets[i];

        break;
      }
    }
  }

  return WiFiClient(result);
}

uint8_t WiFiServer::status() {
  // Deprecated.
  return 0;
}

size_t WiFiServer::write(uint8_t b)
{
  return write(&b, 1);
}

size_t WiFiServer::write(const uint8_t *buffer, size_t size)
{
  size_t written = 0;

  for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
    if (_spawnedSockets[i] != -1) {
      WiFiClient c(_spawnedSockets[i]);

      written += c.write(buffer, size);
    }
  }

  return written;
}

WiFiServer::operator bool()
{
  return (_port != 0 && _socket != -1);
}
