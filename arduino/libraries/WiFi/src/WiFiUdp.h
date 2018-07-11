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

#ifndef WIFIUDP_H
#define WIFIUDP_H

// #include <Udp.h>

class WiFiUDP /*: public UDP*/ {

public:
  WiFiUDP();
  virtual uint8_t begin(uint16_t);
  virtual uint8_t beginMulticast(/*IPAddress*/uint32_t, uint16_t);
  virtual void stop();

  virtual int beginPacket(/*IPAddress*/uint32_t ip, uint16_t port);
  virtual int beginPacket(const char *host, uint16_t port);
  virtual int endPacket();
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buffer, size_t size);

  // using Print::write;

  virtual int parsePacket();
  virtual int available();
  virtual int read();
  virtual int read(unsigned char* buffer, size_t len);
  virtual int read(char* buffer, size_t len) { return read((unsigned char*)buffer, len); };
  virtual int peek();
  virtual void flush();

  virtual /*IPAddress*/ uint32_t remoteIP();
  virtual uint16_t remotePort();

  virtual operator bool() { return _socket != -1; }

private:
  int _socket;
  uint32_t _remoteIp;
  uint16_t _remotePort;

  uint8_t _rcvBuffer[1500];
  uint16_t _rcvIndex;
  uint16_t _rcvSize;
  uint8_t _sndBuffer[1500];
  uint16_t _sndSize;
};

#endif // WIFIUDP_H
