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

#ifndef WIFISERVER_H
#define WIFISERVER_H

#include <sdkconfig.h>

#include <Arduino.h>
// #include <Server.h>

class WiFiClient;

class WiFiServer /*: public Server*/ {
public:
  WiFiServer();
  WiFiServer(uint16_t);
  WiFiClient available(uint8_t* status = NULL);
  void begin();
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  uint8_t status();

  // using Print::write;

  virtual operator bool();

private:
  uint16_t _port;
  int _socket;
  int _spawnedSockets[CONFIG_LWIP_MAX_SOCKETS];
};

#endif // WIFISERVER_H
