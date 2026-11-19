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

#include "WiFiUdp.h"

WiFiUDP::WiFiUDP() :
  _socket(-1),
  _rxRemoteIp(0),
  _rxRemotePort(0),
  _txRemoteIp(0),
  _txRemotePort(0),
  _rcvIndex(0),
  _rcvSize(0),
  _sndSize(0),
  _rxDestinationIp(0),
  _rxDestinationValid(false),
  _rxIsMulticast(false)
{
}

uint8_t WiFiUDP::begin(uint16_t port)
{
  _socket = lwip_socket(AF_INET, SOCK_DGRAM, 0);

  if (_socket < 0) {
    return 0;
  }
  
  int enable = 1;
  _pktInfoSetRc = lwip_setsockopt(
    _socket, IPPROTO_IP, IP_PKTINFO,
    &enable, sizeof(enable)
  );
//  _pktInfoErrno = (_pktInfoSetRc < 0) ? errno : 0; // TEST

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (uint32_t)0;
  addr.sin_port = htons(port);

  if (lwip_bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    lwip_close(_socket);
    _socket = -1;
    return 0;
  }

  int nonBlocking = 1;
  lwip_ioctl(_socket, FIONBIO, &nonBlocking);

  return 1;
}

uint8_t WiFiUDP::beginMulticast(/*IPAddress*/uint32_t ip, uint16_t port)
{
  if (!begin(port)) {
    return 0;
  }

  struct ip_mreq multi;

  multi.imr_multiaddr.s_addr = (uint32_t)ip;
  multi.imr_interface.s_addr = (uint32_t)0;

  lwip_setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multi, sizeof(multi));

  return 1;
}

/* return number of bytes available in the current packet,
   will return zero if parsePacket hasn't been called yet */
int WiFiUDP::available()
{
  return (_rcvSize - _rcvIndex);
}

/* Release any resources being used by this WiFiUDP instance */
void WiFiUDP::stop()
{
  lwip_close(_socket);
  _socket = -1;
}

int WiFiUDP::beginPacket(const char *host, uint16_t port)
{
  uint32_t address;

  if (!WiFi.hostByName(host, address)) {
    return 0;
  }

  return beginPacket(address, port);
}

int WiFiUDP::beginPacket(/*IPAddress*/uint32_t ip, uint16_t port)
{
  _txRemoteIp = ip;
  _txRemotePort = port;

  _sndSize = 0;

  return 1;
}

int WiFiUDP::endPacket()
{
  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = _txRemoteIp;
  addr.sin_port = htons(_txRemotePort);

  if (lwip_sendto(_socket, _sndBuffer, _sndSize, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    return 0;
  }

  return 1;
}

size_t WiFiUDP::write(uint8_t byte)
{
  return write(&byte, 1);
}

size_t WiFiUDP::write(const uint8_t *buffer, size_t size)
{
  size_t written = size;

  if ((_sndSize + size) > sizeof(_sndBuffer)) {
    written = sizeof(_sndBuffer) - _sndSize;
  }

  memcpy(&_sndBuffer[_sndSize], buffer, size);

  _sndSize += written;

  return written;
}

int WiFiUDP::parsePacket()
{
#if 0  
  struct sockaddr_in addr;
  socklen_t addrLen = sizeof(addr);

  _rcvIndex = 0;
  _rcvSize = 0;

  int result = lwip_recvfrom(_socket, _rcvBuffer, sizeof(_rcvBuffer), MSG_DONTWAIT, (struct sockaddr*)&addr, &addrLen);

  if (result <= 0) {
    return 0;
  }

  // delay a bit to allow other tasks to run ...
  vTaskDelay(1);

  _rcvSize = result;
  _rxRemoteIp = addr.sin_addr.s_addr;
  _rxRemotePort = ntohs(addr.sin_port);
#else
  _rcvIndex = 0;
  _rcvSize = 0;

  struct sockaddr_in source = {};
  struct iovec iov = {};

  char control[CMSG_SPACE(sizeof(struct in_pktinfo))] = {};

  struct msghdr msg = {};

  iov.iov_base = _rcvBuffer;
  iov.iov_len = sizeof(_rcvBuffer);

  msg.msg_name = &source;
  msg.msg_namelen = sizeof(source);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof(control);

  int result = lwip_recvmsg(_socket, &msg, MSG_DONTWAIT);

  if (result <= 0)
    return 0;

  uint32_t destinationIp = 0;
  bool destinationValid = false;

  if (!(msg.msg_flags & MSG_CTRUNC)) {
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
      cmsg != nullptr;
      cmsg = CMSG_NXTHDR(&msg, cmsg)) {

      if (cmsg->cmsg_level == IPPROTO_IP &&
        cmsg->cmsg_type == IP_PKTINFO &&
        cmsg->cmsg_len >= CMSG_LEN(sizeof(struct in_pktinfo))) {

        struct in_pktinfo *info =
          (struct in_pktinfo *)CMSG_DATA(cmsg);

        destinationIp = info->ipi_addr.s_addr;
        destinationValid = true;
        break;
      }
    }
  }

#if 0 // XYZZY_TEST_1  
  uint32_t diagnostic = 0x04030201;  // 1.2.3.4: recvmsg succeeded

  if (msg.msg_flags & MSG_CTRUNC) {
    diagnostic = 0x04030202;       // 2.2.3.4: control truncated
  }
  else {
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
         cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {

        diagnostic = 0x04030203;   // 3.2.3.4: some CMSG received

        if (cmsg->cmsg_level == IPPROTO_IP &&
            cmsg->cmsg_type == IP_PKTINFO) {

            diagnostic = 0x04030204; // 4.2.3.4: IP_PKTINFO found

            if (cmsg->cmsg_len >=
                CMSG_LEN(sizeof(struct in_pktinfo))) {

                struct in_pktinfo *info =
                    (struct in_pktinfo *)CMSG_DATA(cmsg);

                diagnostic = info->ipi_addr.s_addr;
            }
            break;
        }
    }
  }
#endif  

  // Preserve the original scheduling behavior.
  vTaskDelay(1);

  _rcvSize = result;
  _rxRemoteIp = source.sin_addr.s_addr;
  _rxRemotePort = ntohs(source.sin_port);

#if 0 // XYZZY_TEST_1
  _rxDestinationIp = diagnostic;  // TEST
#else
  _rxDestinationIp = destinationIp;
#endif

#if 0 // XYZZY_TEST_2
  _rxDestinationIp = 0x00000000;
  uint8_t *p = (uint8_t *)&_rxDestinationIp;
  p[0] = 10;
  p[1] = (_pktInfoSetRc == 0) ? 1 : 0;
  p[2] = (uint8_t)_pktInfoErrno;
  p[3] = (uint8_t)msg.msg_controllen;
#endif    
  
  _rxDestinationValid = destinationValid;

  if (destinationValid) {
    uint32_t ip = ntohl(destinationIp);

    _rxIsMulticast = (ip & 0xF0000000UL) == 0xE0000000UL;
  }

#endif  
           
  return result;
}

int WiFiUDP::read()
{
  uint8_t b;

  if (read(&b, sizeof(b)) < 1) {
    return -1;
  }

  return b;
}

int WiFiUDP::read(unsigned char* buf, size_t size)
{
  if (available() < (int)size) {
    size = available();
  }

  memcpy(buf, &_rcvBuffer[_rcvIndex], size);

  _rcvIndex += size;

  return size;
}

int WiFiUDP::peek()
{
  if (!available()) {
    return -1;
  }

  return _rcvBuffer[_rcvIndex];
}

void WiFiUDP::flush()
{
}

/*IPAddress*/uint32_t WiFiUDP::remoteIP()
{
  return _rxRemoteIp;
}

uint16_t  WiFiUDP::remotePort()
{
  return _rxRemotePort;
}

uint32_t WiFiUDP::destinationIP()
{
  return _rxDestinationIp;
}
