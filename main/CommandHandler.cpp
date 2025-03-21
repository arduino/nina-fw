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

#include <lwip/sockets.h>
#include <driver/spi_common.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiSSLClient.h>
#include <WiFiUdp.h>

#include "CommandHandler.h"

#include <BearSSLClient.h>
#include <BearSSLTrustAnchors.h>
#include <ArduinoECCX08.h>
#include <ArduinoBearSSL.h>
#include "CryptoUtil.h"
#include "ECCX08Cert.h"

#include "esp_log.h"

#ifdef LWIP_PROVIDE_ERRNO
int errno;
#endif

const char FIRMWARE_VERSION[6] = "2.0.0";

/*IPAddress*/uint32_t resolvedHostname;

#define MAX_SOCKETS CONFIG_LWIP_MAX_SOCKETS

uint8_t socketTypes[MAX_SOCKETS];
WiFiClient tcpClients[MAX_SOCKETS];
WiFiUDP udps[MAX_SOCKETS];
WiFiSSLClient tlsClients[MAX_SOCKETS];
WiFiServer tcpServers[MAX_SOCKETS];

WiFiClient bearssl_tcp_client;
BearSSLClient bearsslClient(bearssl_tcp_client, ArduinoIoTCloudTrustAnchor, ArduinoIoTCloudTrustAnchor_NUM);

int setNet(const uint8_t command[], uint8_t response[])
{
  char ssid[32 + 1];

  memset(ssid, 0x00, sizeof(ssid));
  memcpy(ssid, &command[4], command[3]);

  WiFi.begin(ssid);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setPassPhrase(const uint8_t command[], uint8_t response[])
{
  char ssid[32 + 1];
  char pass[64 + 1];

  memset(ssid, 0x00, sizeof(ssid));
  memset(pass, 0x00, sizeof(pass));

  memcpy(ssid, &command[4], command[3]);
  memcpy(pass, &command[5 + command[3]], command[4 + command[3]]);

  WiFi.begin(ssid, pass);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setKey(const uint8_t command[], uint8_t response[])
{
  char ssid[32 + 1];
  char key[26 + 1];

  memset(ssid, 0x00, sizeof(ssid));
  memset(key, 0x00, sizeof(key));

  memcpy(ssid, &command[4], command[3]);
  memcpy(key, &command[7 + command[3]], command[6 + command[3]]);

  WiFi.begin(ssid, key);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setIPconfig(const uint8_t command[], uint8_t response[])
{
  uint32_t ip;
  uint32_t gwip;
  uint32_t mask;

  memcpy(&ip, &command[6], sizeof(ip));
  memcpy(&gwip, &command[11], sizeof(gwip));
  memcpy(&mask, &command[16], sizeof(mask));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  WiFi.config(ip, gwip, mask);

  return 6;
}

int setDNSconfig(const uint8_t command[], uint8_t response[])
{
  uint32_t dns1;
  uint32_t dns2;

  memcpy(&dns1, &command[6], sizeof(dns1));
  memcpy(&dns2, &command[11], sizeof(dns2));

  WiFi.setDNS(dns1, dns2);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setHostname(const uint8_t command[], uint8_t response[])
{
  char hostname[255 + 1];

  memset(hostname, 0x00, sizeof(hostname));
  memcpy(hostname, &command[4], command[3]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  WiFi.hostname(hostname);

  return 6;
}

int setPowerMode(const uint8_t command[], uint8_t response[])
{
  if (command[4]) {
    // low power
    WiFi.lowPowerMode();
  } else {
    // no low power
    WiFi.noLowPowerMode();
  }

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setApNet(const uint8_t command[], uint8_t response[])
{
  char ssid[32 + 1];
  uint8_t channel;

  memset(ssid, 0x00, sizeof(ssid));
  memcpy(ssid, &command[4], command[3]);

  channel = command[5 + command[3]];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (WiFi.beginAP(ssid, channel) != WL_AP_FAILED) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int setApPassPhrase(const uint8_t command[], uint8_t response[])
{
  char ssid[32 + 1];
  char pass[64 + 1];
  uint8_t channel;

  memset(ssid, 0x00, sizeof(ssid));
  memset(pass, 0x00, sizeof(pass));

  memcpy(ssid, &command[4], command[3]);
  memcpy(pass, &command[5 + command[3]], command[4 + command[3]]);
  channel = command[6 + command[3] + command[4 + command[3]]];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (WiFi.beginAP(ssid, pass, channel) != WL_AP_FAILED) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

extern void setDebug(int debug);

int setDebug(const uint8_t command[], uint8_t response[])
{
  setDebug(command[4]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

extern "C" {
  uint8_t temprature_sens_read();
}

int getTemperature(const uint8_t command[], uint8_t response[])
{
  float temperature = (temprature_sens_read() - 32) / 1.8;

  response[2] = 1; // number of parameters
  response[3] = sizeof(temperature); // parameter 1 length

  memcpy(&response[4], &temperature, sizeof(temperature));

  return 9;
}

int getDNSconfig(const uint8_t command[], uint8_t response[])
{
  uint32_t dnsip0 = WiFi.dnsIP();
  uint32_t dnsip1 = WiFi.dnsIP(1);

  response[2] = 2; // number of parameters

  response[3] = 4; // parameter 1 length
  memcpy(&response[4], &dnsip0, sizeof(dnsip0));

  response[8] = 4; // parameter 2 length
  memcpy(&response[9], &dnsip1, sizeof(dnsip1));

  return 14;
}

int getReasonCode(const uint8_t command[], uint8_t response[])
{
  uint8_t reasonCode = WiFi.reasonCode();

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = reasonCode;

  return 6;
}

int getConnStatus(const uint8_t command[], uint8_t response[])
{
  uint8_t status = WiFi.status();

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = status;

  return 6;
}

int getIPaddr(const uint8_t command[], uint8_t response[])
{
  /*IPAddress*/uint32_t ip = WiFi.localIP();
  /*IPAddress*/uint32_t mask = WiFi.subnetMask();
  /*IPAddress*/uint32_t gwip = WiFi.gatewayIP();

  response[2] = 3; // number of parameters

  response[3] = 4; // parameter 1 length
  memcpy(&response[4], &ip, sizeof(ip));

  response[8] = 4; // parameter 2 length
  memcpy(&response[9], &mask, sizeof(mask));

  response[13] = 4; // parameter 3 length
  memcpy(&response[14], &gwip, sizeof(gwip));

  return 19;
}

int getMACaddr(const uint8_t command[], uint8_t response[])
{
  uint8_t mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

  WiFi.macAddress(mac);

  response[2] = 1; // number of parameters
  response[3] = sizeof(mac); // parameter 1 length

  memcpy(&response[4], mac, sizeof(mac));

  return 11;
}

int getCurrSSID(const uint8_t command[], uint8_t response[])
{
  // ssid
  const char* ssid = WiFi.SSID();
  uint8_t ssidLen = strlen(ssid);

  response[2] = 1; // number of parameters
  response[3] = ssidLen; // parameter 1 length

  memcpy(&response[4], ssid, ssidLen);

  return (5 + ssidLen);
}

int getCurrBSSID(const uint8_t command[], uint8_t response[])
{
  uint8_t bssid[6];

  WiFi.BSSID(bssid);

  response[2] = 1; // number of parameters
  response[3] = 6; // parameter 1 length

  memcpy(&response[4], bssid, sizeof(bssid));

  return 11;
}

int getCurrRSSI(const uint8_t command[], uint8_t response[])
{
  int32_t rssi = WiFi.RSSI();

  response[2] = 1; // number of parameters
  response[3] = sizeof(rssi); // parameter 1 length

  memcpy(&response[4], &rssi, sizeof(rssi));

  return 9;
}

int getCurrEnct(const uint8_t command[], uint8_t response[])
{
  uint8_t encryptionType = WiFi.encryptionType();

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = encryptionType;

  return 6;
}

int scanNetworks(const uint8_t command[], uint8_t response[])
{
  int num = WiFi.scanNetworks();
  int responseLength = 3;

  response[2] = num;

  for (int i = 0; i < num; i++) {
    const char* ssid = WiFi.SSID(i);
    int ssidLen = strlen(ssid);

    response[responseLength++] = ssidLen;

    memcpy(&response[responseLength], ssid, ssidLen);
    responseLength += ssidLen;
  }

  return (responseLength + 1);
}

int startServerTcp(const uint8_t command[], uint8_t response[])
{
  uint32_t ip = 0;
  uint16_t port;
  uint8_t socket;
  uint8_t type;

  if (command[2] == 3) {
    memcpy(&port, &command[4], sizeof(port));
    port = ntohs(port);
    socket = command[7];
    type = command[9];
  } else {
    memcpy(&ip, &command[4], sizeof(ip));
    memcpy(&port, &command[9], sizeof(port));
    port = ntohs(port);
    socket = command[12];
    type = command[14];
  }

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (type == 0x00 && tcpServers[socket].begin(port)) {
    socketTypes[socket] = 0x00;
    response[4] = 1;
  } else if (type == 0x01 && udps[socket].begin(port)) {
    socketTypes[socket] = 0x01;
    response[4] = 1;
  } else if (type == 0x03 && udps[socket].beginMulticast(ip, port)) {
    socketTypes[socket] = 0x01;
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int getStateTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (tcpServers[socket]) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int dataSentTcp(const uint8_t command[], uint8_t response[])
{
  // -> no op as write does the work

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int availDataTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];
  uint16_t available = 0;

  if (socketTypes[socket] == 0x00) {
    if (tcpServers[socket]) {

      uint8_t accept = command[6];
      available = 255;

      if (accept) {
        for (int i = 0; i < MAX_SOCKETS; i++) {
          if (socketTypes[i] == 255) {
            WiFiClient client = tcpServers[socket].accept();
            if (client) {
              socketTypes[i] = 0x00;
              tcpClients[i] = client;
              available = i;
            }
            break;
          }
        }
     } else {
      WiFiClient client = tcpServers[socket].available();
      if (client) {
        // try to find existing socket slot
        for (int i = 0; i < MAX_SOCKETS; i++) {
          if (i == socket) {
            continue; // skip this slot
          }

          if (socketTypes[i] == 0x00 && tcpClients[i] == client) {
            available = i;
            break;
          }
        }

        if (available == 255) {
          // book keep new slot

          for (int i = 0; i < MAX_SOCKETS; i++) {
            if (i == socket) {
              continue; // skip this slot
            }

            if (socketTypes[i] == 255) {
              socketTypes[i] = 0x00;
              tcpClients[i] = client;

              available = i;
              break;
            }
          }
        }
      }
     }
    } else {
      available = tcpClients[socket].available();
    }
  } else if (socketTypes[socket] == 0x01) {
    available = udps[socket].available();
  } else if (socketTypes[socket] == 0x02) {
    available = tlsClients[socket].available();
  } else if (socketTypes[socket] == 0x04) {
    available = bearsslClient.available();
  }

  response[2] = 1; // number of parameters
  response[3] = sizeof(available); // parameter 1 length

  memcpy(&response[4], &available, sizeof(available));

  return 7;
}

int getDataTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];
  uint8_t peek = command[6];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (socketTypes[socket] == 0x00) {
    if (peek) {
      response[4] = tcpClients[socket].peek();
    } else {
      response[4] = tcpClients[socket].read();
    }
  } else if (socketTypes[socket] == 0x01) {
    if (peek) {
      response[4] = udps[socket].peek();
    } else {
      response[4] = udps[socket].read();
    }
  } else if (socketTypes[socket] == 0x02) {
    if (peek) {
      response[4] = tlsClients[socket].peek();
    } else {
      response[4] = tlsClients[socket].read();
    }
  } else if (socketTypes[socket] == 0x04) {
    if (peek) {
      response[4] = bearsslClient.peek();
    } else {
      response[4] = bearsslClient.read();
    }
  }

  return 6;
}

static ECCX08CertClass eccx08_cert;
unsigned long getTime();
static void configureECCx08() {
  if (!ECCX08.begin()) {
    ESP_LOGE("ECCX08", "ECCX08.begin() failed");
    return;
  }
  ArduinoBearSSL.onGetTime(getTime);
  ESP_LOGI("ECCX08", "ArduinoBearSSL.getTime() = %lu", ArduinoBearSSL.getTime());

  String device_id;
  if (!CryptoUtil::readDeviceId(ECCX08, device_id, ECCX08Slot::DeviceId)) {
    ESP_LOGE("ECCX08", "Cryptography processor read failure.");
    return;
  }
  ESP_LOGI("ECCX08", "device_id = %s", device_id.c_str());

  if (!CryptoUtil::reconstructCertificate(eccx08_cert, device_id, ECCX08Slot::Key, ECCX08Slot::CompressedCertificate, ECCX08Slot::SerialNumberAndAuthorityKeyIdentifier)) {
    ESP_LOGE("ECCX08", "Cryptography certificate reconstruction failure.");
    return;
  }

  bearsslClient.setEccSlot(static_cast<int>(ECCX08Slot::Key), eccx08_cert.bytes(), eccx08_cert.length());
  ESP_LOGI("ECCX08", "ArduinoBearSSL configured");
}

int startClientTcp(const uint8_t command[], uint8_t response[])
{
  char host[255 + 1];
  uint32_t ip;
  uint16_t port;
  uint8_t socket;
  uint8_t type;

  memset(host, 0x00, sizeof(host));

  if (command[2] == 4) {
    memcpy(&ip, &command[4], sizeof(ip));
    memcpy(&port, &command[9], sizeof(port));
    port = ntohs(port);
    socket = command[12];
    type = command[14];
  } else {
    memcpy(host, &command[4], command[3]);
    memcpy(&ip, &command[5 + command[3]], sizeof(ip));
    memcpy(&port, &command[10 + command[3]], sizeof(port));
    port = ntohs(port);
    socket = command[13 + command[3]];
    type = command[15 + command[3]];
  }

  if (type == 0x00) {
    int result;

    if (host[0] != '\0') {
      result = tcpClients[socket].connect(host, port);
    } else {
      result = tcpClients[socket].connect(ip, port);
    }

    if (result) {
      socketTypes[socket] = 0x00;

      response[2] = 1; // number of parameters
      response[3] = 1; // parameter 1 length
      response[4] = 1;

      return 6;
    } else {
      response[2] = 0; // number of parameters

      return 4;
    }
  } else if (type == 0x01) {
    int result;

    if (host[0] != '\0') {
      result = udps[socket].beginPacket(host, port);
    } else {
      result = udps[socket].beginPacket(ip, port);
    }

    if (result) {
      socketTypes[socket] = 0x01;

      response[2] = 1; // number of parameters
      response[3] = 1; // parameter 1 length
      response[4] = 1;

      return 6;
    } else {
      response[2] = 0; // number of parameters

      return 4;
    }
  } else if (type == 0x02) {
    int result;

    if (host[0] != '\0') {
      result = tlsClients[socket].connect(host, port);
    } else {
      result = tlsClients[socket].connect(ip, port);
    }

    if (result) {
      socketTypes[socket] = 0x02;

      response[2] = 1; // number of parameters
      response[3] = 1; // parameter 1 length
      response[4] = 1;

      return 6;
    } else {
      response[2] = 0; // number of parameters

      return 4;
    }
  } else if (type == 0x04) {
    int result;

    configureECCx08();

    if (host[0] != '\0') {
      result = bearsslClient.connect(host, port);
    } else {
      result = bearsslClient.connect(ip, port);
    }

    if (result) {
      socketTypes[socket] = 0x04;

      response[2] = 1; // number of parameters
      response[3] = 1; // parameter 1 length
      response[4] = 1;

      return 6;
    } else {
      response[2] = 0; // number of parameters

      return 4;
    }
  } else {
    response[2] = 0; // number of parameters

    return 4;
  }
}

int stopClientTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];

  if (socketTypes[socket] == 0x00) {
    tcpClients[socket].stop();
  } else if (socketTypes[socket] == 0x01) {
    udps[socket].stop();
  } else if (socketTypes[socket] == 0x02) {
    tlsClients[socket].stop();
  } else if (socketTypes[socket] == 0x04) {
    bearsslClient.stop();
  }
  socketTypes[socket] = 255;

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int getClientStateTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if ((socketTypes[socket] == 0x00) && tcpClients[socket].connected()) {
    response[4] = 4;
  } else if ((socketTypes[socket] == 0x02) && tlsClients[socket].connected()) {
    response[4] = 4;
  } else if ((socketTypes[socket] == 0x04) && bearsslClient.connected()) {
    response[4] = 4;
  } else {
    socketTypes[socket] = 255;
    response[4] = 0;
  }

  return 6;
}

int disconnect(const uint8_t command[], uint8_t response[])
{
  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  WiFi.disconnect();

  return 6;
}

int getIdxRSSI(const uint8_t command[], uint8_t response[])
{
  // RSSI
  int32_t rssi = WiFi.RSSI(command[4]);

  response[2] = 1; // number of parameters
  response[3] = sizeof(rssi); // parameter 1 length

  memcpy(&response[4], &rssi, sizeof(rssi));

  return 9;
}

int getIdxEnct(const uint8_t command[], uint8_t response[])
{
  uint8_t encryptionType = WiFi.encryptionType(command[4]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = encryptionType;

  return 6;
}

int reqHostByName(const uint8_t command[], uint8_t response[])
{
  char host[255 + 1];

  memset(host, 0x00, sizeof(host));
  memcpy(host, &command[4], command[3]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  resolvedHostname = /*IPAddress(255, 255, 255, 255)*/0xffffffff;
  if (WiFi.hostByName(host, resolvedHostname)) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int getHostByName(const uint8_t command[], uint8_t response[])
{
  response[2] = 1; // number of parameters
  response[3] = 4; // parameter 1 length
  memcpy(&response[4], &resolvedHostname, sizeof(resolvedHostname));

  return 9;
}

int startScanNetworks(const uint8_t command[], uint8_t response[])
{
  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int getFwVersion(const uint8_t command[], uint8_t response[])
{
  response[2] = 1; // number of parameters
  response[3] = sizeof(FIRMWARE_VERSION); // parameter 1 length

  memcpy(&response[4], FIRMWARE_VERSION, sizeof(FIRMWARE_VERSION));

  return 11;
}

int sendUDPdata(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (udps[socket].endPacket()) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int getRemoteData(const uint8_t command[], uint8_t response[])
{
  uint8_t socket = command[4];

  /*IPAddress*/uint32_t ip = /*IPAddress(0, 0, 0, 0)*/0;
  uint16_t port = 0;

  if (socketTypes[socket] == 0x00) {
    ip = tcpClients[socket].remoteIP();
    port = tcpClients[socket].remotePort();
  } else if (socketTypes[socket] == 0x01) {
    ip = udps[socket].remoteIP();
    port = udps[socket].remotePort();
  } else if (socketTypes[socket] == 0x02) {
    ip = tlsClients[socket].remoteIP();
    port = tlsClients[socket].remotePort();
  } else if (socketTypes[socket] == 0x04) {
    ip = static_cast<WiFiClient*>(bearsslClient.getClient())->remoteIP();
    port = static_cast<WiFiClient*>(bearsslClient.getClient())->remotePort();
  }

  response[2] = 2; // number of parameters

  response[3] = 4; // parameter 1 length
  memcpy(&response[4], &ip, sizeof(ip));

  response[8] = 2; // parameter 2 length
  response[9] = (port >> 8) & 0xff;
  response[10] = (port >> 0) & 0xff;

  return 12;
}

int getTime(const uint8_t command[], uint8_t response[])
{
  unsigned long now = WiFi.getTime();

  response[2] = 1; // number of parameters
  response[3] = sizeof(now); // parameter 1 length

  memcpy(&response[4], &now, sizeof(now));

  return 5 + sizeof(now);
}

int getIdxBSSID(const uint8_t command[], uint8_t response[])
{
  uint8_t bssid[6];

  WiFi.BSSID(command[4], bssid);

  response[2] = 1; // number of parameters
  response[3] = 6; // parameter 1 length
  memcpy(&response[4], bssid, sizeof(bssid));

  return 11;
}

int getIdxChannel(const uint8_t command[], uint8_t response[])
{
  uint8_t channel = WiFi.channel(command[4]);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = channel;

  return 6;
}

int setEnt(const uint8_t command[], uint8_t response[])
{
  const uint8_t* commandPtr = &command[3];
  uint8_t eapType;
  char ssid[32 + 1];

  memset(ssid, 0x00, sizeof(ssid));

  // EAP Type - length
  uint16_t eapTypeLen = (commandPtr[0] << 8) | commandPtr[1];
  commandPtr += sizeof(eapTypeLen);

  // EAP Type - data
  memcpy(&eapType, commandPtr, sizeof(eapType));
  commandPtr += sizeof(eapType);

  // SSID - length
  uint16_t ssidLen = (commandPtr[0] << 8) | commandPtr[1];
  commandPtr += sizeof(ssidLen);

  // SSID - data
  memcpy(ssid, commandPtr, ssidLen);
  commandPtr += ssidLen;

  if (eapType == 0) {
    // PEAP/MSCHAPv2
    char username[128 + 1];
    char password[128 + 1];
    char identity[128 + 1];
    const char* rootCA;

    memset(username, 0x00, sizeof(username));
    memset(password, 0x00, sizeof(password));
    memset(identity, 0x00, sizeof(identity));

    // username - length
    uint16_t usernameLen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(usernameLen);

    // username - data
    memcpy(username, commandPtr, usernameLen);
    commandPtr += usernameLen;

    // password - length
    uint16_t passwordLen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(passwordLen);

    // password - data
    memcpy(password, commandPtr, passwordLen);
    commandPtr += passwordLen;

    // identity - length
    uint16_t identityLen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(identityLen);

    // identity - data
    memcpy(identity, commandPtr, identityLen);
    commandPtr += identityLen;

    // rootCA - length
    uint16_t rootCALen = (commandPtr[0] << 8) | commandPtr[1];
    memcpy(&rootCALen, commandPtr, sizeof(rootCALen));
    commandPtr += sizeof(rootCALen);

    // rootCA - data
    rootCA = (const char*)commandPtr;
    commandPtr += rootCALen;

    WiFi.beginEnterprise(ssid, username, password, identity, rootCA);
  } else {
    // EAP-TLS
    const char* cert;
    const char* key;
    char identity[128 + 1];
    const char* rootCA;

    memset(identity, 0x00, sizeof(identity));

    // cert - length
    uint16_t certLen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(certLen);

    // cert - data
    cert = (const char*)commandPtr;
    commandPtr += certLen;

    // key - length
    uint16_t keyLen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(keyLen);

    // key - data
    key = (const char*)commandPtr;
    commandPtr += keyLen;

    // identity - length
    uint16_t identityLen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(identityLen);

    // identity - data
    memcpy(identity, commandPtr, identityLen);
    commandPtr += identityLen;

    // rootCA - length
    uint16_t rootCALen = (commandPtr[0] << 8) | commandPtr[1];
    commandPtr += sizeof(rootCALen);

    // rootCA - data
    rootCA = (const char*)commandPtr;
    commandPtr += rootCALen;

    WiFi.beginEnterpriseTLS(ssid, cert, key, identity, rootCA);
  }

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int sendDataTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket;
  uint16_t length;
  uint16_t written = 0;

  socket = command[5];
  memcpy(&length, &command[6], sizeof(length));
  length = ntohs(length);

  if ((socketTypes[socket] == 0x00) && tcpServers[socket]) {
    written = tcpServers[socket].write(&command[8], length);
  } else if (socketTypes[socket] == 0x00) {
    written = tcpClients[socket].write(&command[8], length);
  } else if (socketTypes[socket] == 0x02) {
    written = tlsClients[socket].write(&command[8], length);
  } else if (socketTypes[socket] == 0x04) {
    written = bearsslClient.write(&command[8], length);
  }

  response[2] = 1; // number of parameters
  response[3] = sizeof(written); // parameter 1 length
  memcpy(&response[4], &written, sizeof(written));

  return 7;
}

int getDataBufTcp(const uint8_t command[], uint8_t response[])
{
  uint8_t socket;
  uint16_t length;
  int read = 0;

  socket = command[5];
  memcpy(&length, &command[8], sizeof(length));

  if (socketTypes[socket] == 0x00) {
    read = tcpClients[socket].read(&response[5], length);
  } else if (socketTypes[socket] == 0x01) {
    read = udps[socket].read(&response[5], length);
  } else if (socketTypes[socket] == 0x02) {
    read = tlsClients[socket].read(&response[5], length);
  } else if (socketTypes[socket] == 0x04) {
    read = bearsslClient.read(&response[5], length);
  }

  if (read < 0) {
    read = 0;
  }

  response[2] = 1; // number of parameters
  response[3] = (read >> 8) & 0xff; // parameter 1 length
  response[4] = (read >> 0) & 0xff;

  return (6 + read);
}

int insertDataBuf(const uint8_t command[], uint8_t response[])
{
  uint8_t socket;
  uint16_t length;

  socket = command[5];
  memcpy(&length, &command[6], sizeof(length));
  length = ntohs(length);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length

  if (udps[socket].write(&command[8], length) != 0) {
    response[4] = 1;
  } else {
    response[4] = 0;
  }

  return 6;
}

int ping(const uint8_t command[], uint8_t response[])
{
  uint32_t ip;
  uint8_t ttl;
  int16_t result;

  memcpy(&ip, &command[4], sizeof(ip));
  ttl = command[9];

  result = WiFi.ping(ip, ttl);

  response[2] = 1; // number of parameters
  response[3] = sizeof(result); // parameter 1 length
  memcpy(&response[4], &result, sizeof(result));

  return 7;
}

int getSocket(const uint8_t command[], uint8_t response[])
{
  uint8_t result = 255;

  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (socketTypes[i] == 255) {
      result = i;
      break;
    }
  }

  response[2] = 1; // number of parameters
  response[3] = sizeof(result); // parameter 1 length
  response[4] = result;

  return 6;
}

int setPinMode(const uint8_t command[], uint8_t response[])
{
  uint8_t pin = command[4];
  uint8_t mode = command[6];

  pinMode(pin, mode);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setDigitalWrite(const uint8_t command[], uint8_t response[])
{
  uint8_t pin = command[4];
  uint8_t value = command[6];

  digitalWrite(pin, value);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setAnalogWrite(const uint8_t command[], uint8_t response[])
{
  uint8_t pin = command[4];
  uint8_t value = command[6];

  analogWrite(pin, value);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int getDigitalRead(const uint8_t command[], uint8_t response[])
{
  uint8_t pin = command[4];

  int const pin_status = digitalRead(pin);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (uint8_t)pin_status;

  return 6;
}

extern "C" {
#include <driver/adc.h>
}

int getAnalogRead(const uint8_t command[], uint8_t response[])
{
  uint8_t adc_channel = command[4];

  /* Initialize the ADC. */
  adc_gpio_init(ADC_UNIT_1, (adc_channel_t)adc_channel);
  /* Set maximum analog bit-width = 12 bit. */
  adc1_config_width(ADC_WIDTH_BIT_12);
  /* Configure channel attenuation. */
  adc1_config_channel_atten((adc1_channel_t)adc_channel, ADC_ATTEN_DB_11);
  /* Read the analog value from the pin. */
  uint16_t const adc_raw = adc1_get_raw((adc1_channel_t)adc_channel);

  response[2] = 1; // number of parameters
  response[3] = sizeof(adc_raw); // parameter 1 length = 2 bytes
  memcpy(&response[4], &adc_raw, sizeof(adc_raw));

  return 7;
}

int writeFile(const uint8_t command[], uint8_t response[]) {
  char filename[32 + 1];
  size_t len;
  size_t offset;

  memcpy(&offset, &command[4], command[3]);
  memcpy(&len, &command[5 + command[3]], command[4 + command[3]]);

  memset(filename, 0x00, sizeof(filename));
  memcpy(filename, &command[6 + command[3] + command[4 + command[3]]], command[5 + command[3] + command[4 + command[3]]]);

  FILE* f = fopen(filename, "ab+");
  if (f == NULL) {
    return -1;
  }

  fseek(f, offset, SEEK_SET);
  const uint8_t* data = &command[7 + command[3] + command[4 + command[3]] + command[5 + command[3] + command[4 + command[3]]]];

  int ret = fwrite(data, 1, len, f);
  fclose(f);

  return ret;
}

int readFile(const uint8_t command[], uint8_t response[]) {
  char filename[32 + 1];
  size_t len;
  size_t offset;

  memcpy(&offset, &command[4], command[3]);
  memcpy(&len, &command[5 + command[3]], command[4 + command[3]]);

  memset(filename, 0x00, sizeof(filename));
  memcpy(filename, &command[6 + command[3] + command[4 + command[3]]], command[5 + command[3] + command[4 + command[3]]]);

  FILE* f = fopen(filename, "rb");
  if (f == NULL) {
    return -1;
  }
  fseek(f, offset, SEEK_SET);
  fread(&response[4], len, 1, f);
  fclose(f);

  response[2] = 1; // number of parameters
  response[3] = len; // parameter 1 length

  return len + 5;
}

int deleteFile(const uint8_t command[], uint8_t response[]) {
  char filename[32 + 1];
  size_t len;
  size_t offset;

  memcpy(&offset, &command[4], command[3]);
  memcpy(&len, &command[5 + command[3]], command[4 + command[3]]);

  memset(filename, 0x00, sizeof(filename));
  memcpy(filename, &command[6 + command[3] + command[4 + command[3]]], command[5 + command[3] + command[4 + command[3]]]);

  struct stat st;
  if (stat(filename, &st) == 0) {
    // Delete it if it exists
    unlink(filename);
  }
  return 0;
}

#include <driver/uart.h>

int applyOTA(const uint8_t command[], uint8_t response[]) {
#ifdef UNO_WIFI_REV2

  const char* filename = "/fs/UPDATE.BIN";
  FILE* updateFile = fopen(filename, "rb");

  // init uart and write update to 4809
  uart_config_t uart_config;

  uart_config.baud_rate = 115200;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 122;
  uart_config.use_ref_tick = true;

  uart_param_config(UART_NUM_1, &uart_config);

  uart_set_pin(UART_NUM_1,
                 1, // tx
                 3, // rx
                 UART_PIN_NO_CHANGE, // rts
                 UART_PIN_NO_CHANGE); //cts

  uart_driver_install(UART_NUM_1, 1024, 0, 20, NULL, 0);

  struct stat st;
  stat(filename, &st);

  int retries = 0;

  size_t remaining =  st.st_size % 1024;
  for (int i=0; i<st.st_size; i++) {
    uint8_t c;
    uint8_t d;

    fread(&c, 1, 1, updateFile);
    retries = 0;
    while (retries == 0 || (c != d && retries < 100)) {
      uart_write_bytes(UART_NUM_1, (const char*)&c, 1);
      uart_read_bytes(UART_NUM_1, &d, 1, 10);
      retries++;
    }
    if (retries >= 100) {
      goto exit;
    }
  }
  // send remaining bytes (to reach page size) as 0xFF
  for (int i=0; i<remaining + 10; i++) {
    uint8_t c = 0xFF;
    uint8_t d;
    retries = 0;
    while (retries == 0 || (c != d && retries < 100)) {
      uart_write_bytes(UART_NUM_1, (const char*)&c, 1);
      uart_read_bytes(UART_NUM_1, &d, 1, 10);
      retries++;
    }
  }

  // delay a bit before restarting, in case the flashing isn't yet over
  delay(200);

  pinMode(19, OUTPUT);
  digitalWrite(19, HIGH);
  delay(200);
  digitalWrite(19, LOW);
  pinMode(19, INPUT);

exit:
  fclose(updateFile);
  unlink(filename);

  return 0;
#else
  return 0;
#endif
}

int renameFile(const uint8_t command[], uint8_t response[]) {
  char old_file_name[64 + 1] = {0};
  char new_file_name[64 + 1] = {0};

  memset(old_file_name, 0, sizeof(old_file_name));
  memcpy(old_file_name, &command[4], command[3]);

  memset(new_file_name, 0, sizeof(new_file_name));
  memcpy(new_file_name, &command[5 + command[3]], command[4 + command[3]]);

  errno = 0;
  rename(old_file_name, new_file_name);

  /* Set up the response packet containing the ERRNO error number */
  response[2] = 1;      /* Number of parameters */
  response[3] = 1;      /* Length of parameter 1 */
  response[4] = errno;  /* The actual payload */

  return 6;
}

int existsFile(const uint8_t command[], uint8_t response[]) {
  char filename[32 + 1];
  size_t len;
  size_t offset;

  memcpy(&offset, &command[4], command[3]);
  memcpy(&len, &command[5 + command[3]], command[4 + command[3]]);

  memset(filename, 0x00, sizeof(filename));
  memcpy(filename, &command[6 + command[3] + command[4 + command[3]]], command[5 + command[3] + command[4 + command[3]]]);

  int ret = -1;

  struct stat st;
  ret = stat(filename, &st);
  if (ret != 0) {
    st.st_size = -1;
  }
  memcpy(&response[4], &(st.st_size), sizeof(st.st_size));

  response[2] = 1; // number of parameters
  response[3] = sizeof(st.st_size); // parameter 1 length

  return 10;
}

int downloadFile(const uint8_t command[], uint8_t response[]) {
  char url[64 + 1];
  char filename[64 + 1];

  memset(url, 0x00, sizeof(url));
  memset(filename, 0x00, sizeof(filename));

  memcpy(url, &command[4], command[3]);
  memcpy(filename, "/fs/", strlen("/fs/"));
  memcpy(&filename[strlen("/fs/")], &command[5 + command[3]], command[4 + command[3]]);

  FILE* f = fopen(filename, "w");
  downloadAndSaveFile(url, f, 0);
  fclose(f);

  return 0;
}

/**
 * Static table used for the table_driven implementation.
 */
static const uint32_t crc_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc_update(uint32_t crc, const void * data, size_t data_len)
{
  const unsigned char *d = (const unsigned char *)data;
  unsigned int tbl_idx;

  while (data_len--) {
    tbl_idx = (crc ^ *d) & 0xff;
    crc = (crc_table[tbl_idx] ^ (crc >> 8)) & 0xffffffff;
    d++;
  }

  return crc & 0xffffffff;
}

int downloadOTA(const uint8_t command[], uint8_t response[])
{
  static const char * OTA_TAG = "OTA";
  static const char * OTA_FILE = "/fs/UPDATE.BIN.LZSS";
  static const char * OTA_TEMP_FILE = "/fs/UPDATE.BIN.LZSS.TMP";

  enum OTA_Error {
    ERR_NO_ERROR = 0,
    ERR_OPEN     = 1,
    ERR_LENGTH   = 2,
    ERR_CRC      = 3,
    ERR_RENAME   = 4,
  };

  union {
    struct __attribute__((packed)) {
      uint32_t len;
      uint32_t crc32;
    } header;
    uint8_t buf[sizeof(header)];
  } ota_header;

  int ota_size, c;
  uint32_t crc32;

  /* Retrieve the URL parameter. */
  char url[128 + 1];
  memset(url, 0, sizeof(url));
  memcpy(url, &command[4], command[3]);
  ESP_LOGI(OTA_TAG, "url: %s", url);

  /* Set up the response packet. */
  response[2] = 1; /* Number of parameters */
  response[3] = 1; /* Length of parameter 1 */
  response[4] = ERR_NO_ERROR; /* The actual payload */

  /* Download the OTA file */
  FILE * f = fopen(OTA_TEMP_FILE, "w+");
  if (!f) {
    ESP_LOGE(OTA_TAG, "fopen(..., \"w+\") error: %d", ferror(f));
    response[4] = ERR_OPEN;
    goto ota_cleanup;
  }
  downloadAndSaveFile(url, f, 0);

  /* Determine size of downloaded file. */
  ota_size = ftell(f) - sizeof(ota_header.buf);
  /* Reposition file pointer at start of file. */
  rewind(f);
  /* Read the OTA header. */
  fread(ota_header.buf, sizeof(ota_header.buf), 1, f);
  ESP_LOGI(OTA_TAG, "ota image length = %d", ota_header.header.len);
  ESP_LOGI(OTA_TAG, "ota image crc32 = %X", ota_header.header.crc32);

  /* Check length. */
  if (ota_header.header.len != ota_size) {
    ESP_LOGE(OTA_TAG, "error ota length: expected %d, actual %d", ota_header.header.len, ota_size);
    response[4] = ERR_LENGTH;
    goto ota_cleanup;
  }

  /* Init CRC */
  crc32 = 0xFFFFFFFF;
  /* Calculate CRC */
  c = fgetc(f);
  while (c != EOF) {
    crc32 = crc_update(crc32, &c, 1);
    c = fgetc(f);
  }
  /* Finalise CRC */
  crc32 ^= 0xFFFFFFFF;

  /* Check CRC. */
  if (ota_header.header.crc32 != crc32) {
    ESP_LOGE(OTA_TAG, "error ota crc: expected %X, actual %X", ota_header.header.crc32, crc32);
    response[4] = ERR_CRC;
    goto ota_cleanup;
  }

  /* Close the file. */
  fclose(f);

  /* Rename in case of success. */
  errno = 0;
  rename(OTA_TEMP_FILE, OTA_FILE);
  if (errno) {
    ESP_LOGE(OTA_TAG, "rename(...) error: %d", errno);
    response[4] = ERR_RENAME;
    goto ota_cleanup;
  }

  return 6;

ota_cleanup:
  fclose(f);
  unlink(OTA_TEMP_FILE);
  return 6;
}

//
// Low-level BSD-like sockets functions
//
int socket_socket(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0   >
  //[1]     Command     < 1 byte >
  //[2]     N args      < 1 byte >
  //[3]     type size   < 1 byte >
  //[4]     type        < 1 byte >
  //[5]     proto size  < 1 byte >
  //[6]     proto       < 1 byte >

  uint8_t type = command[4];
  uint8_t proto = command[6];

  errno = 0;
  int8_t ret = lwip_socket(AF_INET, type, proto);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 255 : ret;
  return 6;
}

int socket_close(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0   >
  //[1]     Command     < 1 byte >
  //[2]     N args      < 1 byte >
  //[3]     sock size   < 1 byte >
  //[4]     sock        < 1 byte >
  uint8_t sock = command[4];

  errno = 0;
  int ret = lwip_close_r(sock);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 0 : 1;
  return 6;
}

int socket_errno(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0   >
  //[1]     Command     < 1 byte >
  //[2]     N args      < 1 byte >
  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = errno;
  return 6;
}

int socket_bind(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0    >
  //[1]     Command     < 1 byte  >
  //[2]     N args      < 1 byte  >
  //[3]     sock size   < 1 byte  >
  //[4]     sock        < 1 byte  >
  //[5]     port size   < 1 byte  >
  //[6..7]  port        < 2 bytes >
  uint8_t sock = command[4];
  uint16_t port = *((uint16_t *) &command[6]);

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = (uint32_t) 0;
  addr.sin_port = port;

  errno = 0;
  int ret = lwip_bind_r(sock, (struct sockaddr*) &addr, sizeof(addr));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 0 : 1;
  return 6;
}

int socket_listen(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START    < 0xE0   >
  //[1]     Command      < 1 byte >
  //[2]     N args       < 1 byte >
  //[3]     sock size    < 1 byte >
  //[4]     sock value   < 1 byte >
  //[5]     backlog size < 1 byte >
  //[6]     backlog      < 1 byte >
  uint8_t sock = command[4];
  uint8_t backlog = command[6];

  errno = 0;
  int ret = lwip_listen_r(sock, backlog);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 0 : 1;
  return 6;
}

int socket_accept(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0   >
  //[1]     Command     < 1 byte >
  //[2]     N args      < 1 byte >
  //[3]     sock size   < 1 byte >
  //[4]     sock        < 1 byte >
  uint8_t sock = command[4];

  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  errno = 0;
  int8_t ret = lwip_accept_r(sock, (struct sockaddr *) &addr, &addr_len);

  response[2] = 3; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 255 : ret;

  // Remote addr
  response[5] = 4; // parameter 2 length
  memcpy(&response[6], &addr.sin_addr.s_addr, 4);

  // Remote port
  response[10] = 2; // parameter 3 length
  response[11] = (addr.sin_port >> 8) & 0xff;
  response[12] = (addr.sin_port >> 0) & 0xff;

  return 14;
}

int socket_connect(const uint8_t command[], uint8_t response[])
{
  //[0]      CMD_START  < 0xE0    >
  //[1]      Command    < 1 byte  >
  //[2]      N args     < 1 byte  >
  //[3]      sock size  < 1 byte  >
  //[4]      sock       < 1 byte  >
  //[5]      ip size    < 1 byte  >
  //[6..9]   ip         < 4 bytes >
  //[10]     port size  < 1 byte  >
  //[11..12] port       < 2 bytes >
  uint8_t sock = command[4];
  uint32_t ip = *((uint32_t *) &command[6]);
  uint16_t port = *((uint16_t *) &command[11]);

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ip;
  addr.sin_port = port;

  errno = 0;
  int ret = lwip_connect_r(sock, (struct sockaddr*)&addr, sizeof(addr));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 0 : 1;
  return 6;
}

int socket_send(const uint8_t command[], uint8_t response[])
{
  //[0]      CMD_START  < 0xE0    >
  //[1]      Command    < 1 byte  >
  //[2]      N args     < 1 byte  >
  //[3..4]   sock size  < 2 bytes >
  //[5]      sock       < 1 byte  >
  //[6..7]   buff size  < 2 bytes >
  //[8]      buff       < n bytes >
  uint8_t sock = command[5];
  uint16_t size = lwip_ntohs(*((uint16_t *) &command[6]));

  errno = 0;
  int16_t ret = lwip_send_r(sock, &command[8], size, 0);
  ret = (ret < 0) ? 0 : ret;

  response[2] = 1; // number of parameters
  response[3] = 2;
  response[4] = (ret >> 8) & 0xff; // parameter 1 length
  response[5] = (ret >> 0) & 0xff;
  return 7;
}

int socket_recv(const uint8_t command[], uint8_t response[])
{
  //[0]      CMD_START  < 0xE0    >
  //[1]      Command    < 1 byte  >
  //[2]      N args     < 1 byte  >
  //[3]      sock size  < 1 byte  >
  //[4]      sock       < 1 byte  >
  //[5]      recv size  < 1 byte  >
  //[6..7]   recv       < 2 bytes >
  uint8_t sock = command[4];
  uint16_t size = *((uint16_t *) &command[6]);
  size = LWIP_MIN(size, (SPI_MAX_DMA_LEN-16));

  errno = 0;
  int16_t ret = lwip_recv_r(sock, &response[5], size, 0);
  ret = (ret < 0) ? 0 : ret;

  response[2] = 1; // number of parameters
  response[3] = (ret >> 8) & 0xff; // parameter 1 length
  response[4] = (ret >> 0) & 0xff;
  return 6 + ret;
}

int socket_sendto(const uint8_t command[], uint8_t response[])
{
  //[0]      CMD_START  < 0xE0    >
  //[1]      Command    < 1 byte  >
  //[2]      N args     < 1 byte  >
  //[3..4]   sock size  < 2 bytes >
  //[5]      sock       < 1 byte  >
  //[6..7]   ip size    < 2 bytes >
  //[8..11]  ip         < 4 bytes >
  //[12..13] port size  < 2 bytes >
  //[14..15] port       < 2 bytes >
  //[16..17] buff size  < 2 bytes >
  //[18]     buff       < n bytes >
  uint8_t sock = command[5];
  uint32_t ip = *((uint32_t *) &command[8]);
  uint16_t port = *((uint16_t *) &command[14]);
  uint16_t size = lwip_ntohs(*((uint16_t *) &command[16]));

  struct sockaddr_in addr;
  memset(&addr, 0x00, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ip;
  addr.sin_port = port;

  errno = 0;
  int16_t ret = lwip_sendto_r(sock, &command[18], size, 0, (struct sockaddr*)&addr, sizeof(addr));
  ret = (ret < 0) ? 0 : ret;

  response[2] = 1; // number of parameters
  response[3] = 2;
  response[4] = (ret >> 8) & 0xff; // parameter 1 length
  response[5] = (ret >> 0) & 0xff;
  return 7;
}

int socket_recvfrom(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0   >
  //[1]     Command     < 1 byte >
  //[2]     N args      < 1 byte >
  //[3]     sock size   < 1 byte >
  //[4]     sock        < 1 byte >
  //[5]     recv size   < 1 byte >
  //[6..7]  recv        < 2 bytes >
  uint8_t sock = command[4];
  uint16_t size = *((uint16_t *) &command[6]);

  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  errno = 0;
  int16_t ret = lwip_recvfrom_r(sock, &response[15], size, 0, (struct sockaddr *) &addr, &addr_len);
  ret = (ret < 0) ? 0 : ret;

  response[2] = 3; // number of parameters

  // Remote addr
  response[3] = 0; // parameter 1 length
  response[4] = 4; // parameter 1 length
  memcpy(&response[5], &addr.sin_addr.s_addr, 4);

  // Remote port
  response[9]  = 0; // parameter 3 length
  response[10] = 2; // parameter 3 length
  response[11] = (addr.sin_port >> 8) & 0xff;
  response[12] = (addr.sin_port >> 0) & 0xff;

  // Received buffer
  response[13] = (ret >> 8) & 0xff; // parameter 4 length
  response[14] = (ret >> 0) & 0xff;
  return 16 + ret;
}

int socket_ioctl(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START    < 0xE0    >
  //[1]     Command      < 1 byte  >
  //[2]     N args       < 1 byte  >
  //[3]     sock size    < 1 byte  >
  //[4]     sock         < 1 byte  >
  //[5]     cmd size     < 1 byte  >
  //[6-9]   cmd          < 4 bytes >
  //[10]    arg size     < 1 byte  >
  //[11]    arg          < n bytes >
  uint8_t sock = command[4];
  uint32_t cmd = *((uint32_t *) &command[6]);
  uint8_t size = LWIP_MIN(command[10], IOCPARM_MASK);

  uint8_t argval[IOCPARM_MASK];
  memcpy(argval, &command[11], size);

  errno = 0;
  int ret = lwip_ioctl_r(sock, cmd, argval);
  if (ret == -1) {
      size = 0;
  }

  response[2] = 1; // number of parameters
  response[3] = size; // parameter 1 length
  // Note: in/out command, always return something.
  memcpy(&response[4], argval, size);
  return 5 + size;
}

#define SOCKET_POLL_RD       (0x01)
#define SOCKET_POLL_WR       (0x02)
#define SOCKET_POLL_ERR      (0x04)
#define SOCKET_POLL_FAIL     (0x80)

int socket_poll(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START    < 0xE0   >
  //[1]     Command      < 1 byte >
  //[2]     N args       < 1 byte >
  //[3]     sock size    < 1 byte >
  //[4]     sock         < 1 byte >
  uint8_t sock = command[4];

  fd_set rset, wset, xset;
  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_ZERO(&wset);

  FD_SET(sock, &rset);
  FD_SET(sock, &wset);
  FD_SET(sock, &xset);

  struct timeval tv = {
    .tv_sec  = 0,
    .tv_usec = 0,
  };

  errno = 0;
  int ret = lwip_select(sock + 1, &rset, &wset, &xset, &tv);

  uint8_t flags = 0;
  if (FD_ISSET(sock, &rset)) {
    flags |= SOCKET_POLL_RD;
  }

  if (FD_ISSET(sock, &wset)) {
    flags |= SOCKET_POLL_WR;
  }

  if (FD_ISSET(sock, &xset)) {
    flags |= SOCKET_POLL_ERR;
  }

  if (ret == -1) {
    flags |= SOCKET_POLL_FAIL;
  }

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = flags;
  return 6;
}

int socket_setsockopt(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START    < 0xE0    >
  //[1]     Command      < 1 byte  >
  //[2]     N args       < 1 byte  >
  //[3]     sock size    < 1 byte  >
  //[4]     sock         < 1 byte  >
  //[5]     optname size < 1 byte  >
  //[6-9]   optname      < 4 bytes >
  //[10]    optlen       < 1 byte  >
  //[11]    optval       < n bytes >
  uint8_t sock = command[4];
  uint32_t optname = *((uint32_t *) &command[6]);
  uint8_t optlen = LWIP_MIN(command[10], LWIP_SETGETSOCKOPT_MAXOPTLEN);
  uint8_t optval[LWIP_SETGETSOCKOPT_MAXOPTLEN];
  memcpy(&optval, &command[11], optlen);

  errno = 0;
  int ret = lwip_setsockopt_r(sock, SOL_SOCKET, optname, optval, optlen);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 0 : 1;
  return 6;
}

int socket_getsockopt(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START    < 0xE0    >
  //[1]     Command      < 1 byte  >
  //[2]     N args       < 1 byte  >
  //[3]     sock size    < 1 byte  >
  //[4]     sock         < 1 byte  >
  //[5]     optname size < 1 byte  >
  //[6-9]   optname      < 4 bytes >
  //[10]    optlen  size < 1 byte  >
  //[11]    optlen       < 1 byte  >
  uint8_t sock = command[4];
  uint32_t optname = *((uint32_t *) &command[6]);
  socklen_t optlen = LWIP_MIN(command[11], LWIP_SETGETSOCKOPT_MAXOPTLEN);
  uint8_t optval[LWIP_SETGETSOCKOPT_MAXOPTLEN];

  errno = 0;
  int ret = lwip_getsockopt_r(sock, SOL_SOCKET, optname, optval, &optlen);
  if (ret == -1) {
      optlen = 0;
  }
  response[2] = 1; // number of parameters
  response[3] = optlen; // parameter 1 length
  memcpy(&response[4], optval, optlen);
  return 5 + optlen;
}

int socket_getpeername(const uint8_t command[], uint8_t response[])
{
  //[0]     CMD_START   < 0xE0   >
  //[1]     Command     < 1 byte >
  //[2]     N args      < 1 byte >
  //[3]     sock size   < 1 byte >
  //[4]     sock        < 1 byte >
  uint8_t sock = command[4];

  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  errno = 0;
  int ret = lwip_getpeername(sock, (struct sockaddr *) &addr, &addr_len);

  response[2] = 3; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = (ret == -1) ? 0 : 1;

  // Remote addr
  response[5] = 4; // parameter 2 length
  memcpy(&response[6], &addr.sin_addr.s_addr, 4);

  // Remote port
  response[10] = 2; // parameter 3 length
  response[11] = (addr.sin_port >> 8) & 0xff;
  response[12] = (addr.sin_port >> 0) & 0xff;

  return 14;
}

/*
 * Preferences API
 */
#include "Preferences.h"

Preferences preferences;
const char PREF_TAG[] = "preferences";

int pref_begin(const uint8_t command[], uint8_t response[])
{

  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         store_name size             < 1 byte >
  //[4..n]      store_name                  < n byte >
  //[n+1]       readonly                    < 1 byte >
  //[n+2]       partition label size        < 1 byte >
  //[n+3..n+m]  partition label             < m byte >

  uint8_t nargs = command[2];
  char store_name[32];
  char partition_label[32];
  const uint8_t* partition_label_ptr = nullptr;
  bool readonly=false;

  // command_ptr points to the next argument, in this case
  // it points to the length of store_name string
  const uint8_t* command_ptr = &command[3];

  if(nargs < 1 && nargs > 3) {
    ESP_LOGE(PREF_TAG, "Prefrences begin wrong number of arguments");
    response[4] = 255;
    goto error;
  }

  memset(store_name, 0x00, sizeof(store_name));
  memcpy(store_name, command_ptr+1, *command_ptr);
  store_name[*command_ptr] = '\0';

  // move the pointer to the next argument, by adding the length
  // of store_name string
  command_ptr += *command_ptr + 1;

  if(nargs > 1) {
    command_ptr++; // the first byte contains the length (that is 1) of the next byte
    readonly = *command_ptr;
    command_ptr++;
  }

  if(nargs > 2) {
    memset(partition_label, 0x00, sizeof(partition_label));
    memcpy(partition_label, command_ptr+1, *command_ptr);
    partition_label[*command_ptr] = '\0';

    partition_label_ptr = command_ptr;
  }

  response[4] = preferences.begin(store_name, readonly, (char*)partition_label_ptr) ? 0 : 1;

error:
  response[2] = 1;          // number of parameters
  response[3] = 1;          // length of first parameter

  return 6;
}

int pref_end(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >

  preferences.end();

  response[2] = 1;          // number of parameters
  response[3] = 1;          // length of first parameter
  response[4] = 1;

  return 6;
}

int pref_clear(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >

  response[2] = 1;                   // number of parameters
  response[3] = 1;                   // length of first parameter
  response[4] = preferences.clear() ? 0 : 1; // result of Preferences clear operation

  // response has to start ad position 2, and has to take into account
  // 0xee that is put after the function being called
  return 6;
}

int pref_remove(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         key size                    < 1 byte >
  //[4..n]      key                         < n byte >

  uint8_t nargs = command[2];
  char key[16];

  // command_ptr points to the next argument, in this case
  // it points to the length of key string
  const uint8_t* command_ptr = &command[3];

  if(nargs != 1) {
    ESP_LOGE(PREF_TAG, "Prefrences remove wrong number of arguments");
    response[4] = 255;
    goto error;
  }

  memset(key, 0x00, sizeof(key));
  memcpy(key, command_ptr+1, *command_ptr);
  key[*command_ptr] = '\0';

  response[4] = preferences.remove(key) ? 0 : 1; // result of Preferences end operation
error:
  response[2] = 1;                 // number of parameters
  response[3] = 1;                 // length of first parameter

  // response has to start ad position 2, and has to take into account
  // 0xee that is put after the function being called
  return 6;
}

int pref_len(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         key size                    < 1 byte >
  //[4..n]      key                         < n byte >

  uint8_t nargs = command[2];
  char key[16];

  // command_ptr points to the next argument, in this case
  // it points to the length of key string
  const uint8_t* command_ptr = &command[3];

  // restricting the return as 32 bit integer as it is enough
  uint32_t len = 0;

  if(nargs != 1) {
    ESP_LOGE(PREF_TAG, "Prefrences length wrong number of arguments");
    response[2] = 1;
    response[3] = 1;
    response[4] = 255;
    return 6;
  }

  memset(key, 0x00, sizeof(key));
  memcpy(key, command_ptr+1, *command_ptr);
  key[*command_ptr] = '\0';

  len = preferences.getBytesLength(key);

  response[2] = 1;          // number of parameters
  response[3] = 4;          // length of first parameter

  // write the result in big endian into the response buffer
  response[4] = (len >> 0)  & 0xff;
  response[5] = (len >> 8)  & 0xff;
  response[6] = (len >> 16) & 0xff;
  response[7] = (len >> 24) & 0xff;

  return 9;
}

int pref_stat(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >

  // restricting the return as 32 bit integer as it is enough
  uint32_t res = 0;

  res = preferences.freeEntries();

  response[2] = 1;          // number of parameters
  response[3] = 4;          // length of first parameter

  // write the result in big endian into the response buffer
  response[4] = (res >> 0)  & 0xff;
  response[5] = (res >> 8)  & 0xff;
  response[6] = (res >> 16) & 0xff;
  response[7] = (res >> 24) & 0xff;

  return 9;
}

int pref_put(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         key size                    < 1 byte >
  //[4..n]      key                         < n byte >
  //[n]         type                        < 1 byte >
  //[n+1]       len                         < 1 byte >
  //[n+2..sizeof(type)]                     < sizeof(type)
  //            value                         or len byte >

  uint8_t nargs = command[2];
  char key[16];
  uint16_t len;

  // we are going to store the value (if not array type) in a 64 bit integer, because it is easier to handle
  uint64_t value=0;

  // command_ptr points to the next argument, in this case
  // it points to the length of key string
  const uint8_t* command_ptr = &command[3];

  // restricting the return as 32 bit integer as it is enough
  size_t res = 0;

  if(nargs != 3) {
    ESP_LOGE(PREF_TAG, "Prefrences put wrong number of arguments");
    response[2] = 1;
    response[3] = 1;
    response[4] = 255;
    return 6;
  }

  memset(key, 0x00, sizeof(key));
  memcpy(key, command_ptr+1, *command_ptr);
  key[*command_ptr] = '\0';

  // next argument
  command_ptr += *command_ptr + 1;

  command_ptr++; // The first byte contains the length of the parameter, which is 1
  PreferenceType type = (PreferenceType)*command_ptr;
  command_ptr++;

  // extract length
  len = command_ptr[0]<<8 | command_ptr[1];
  command_ptr+=2;

  // extract value convert from bigendian, TODO not forr array types
  for(uint8_t i=0; i<len && type != PT_BLOB && type != PT_STR; i++) {
    value |= (((uint64_t)command_ptr[i]) << (i << 3));
  }

  switch(type) {
    case PT_I8:
      res = preferences.putChar(key, static_cast<int8_t>(value));
      break;
    case PT_U8:
      res = preferences.putUChar(key, static_cast<uint8_t>(value));
      break;
    case PT_I16:
      res = preferences.putShort(key, static_cast<int16_t>(value));
      break;
    case PT_U16:
      res = preferences.putUShort(key, static_cast<uint16_t>(value));
      break;
    case PT_I32:
      res = preferences.putInt(key, static_cast<int32_t>(value));
      break;
    case PT_U32:
      res = preferences.putUInt(key, static_cast<uint32_t>(value));
      break;
    case PT_I64:
      res = preferences.putLong64(key, static_cast<int64_t>(value));
      break;
    case PT_U64:
      res = preferences.putULong64(key, static_cast<uint64_t>(value));
      break;
    case PT_STR:
      // for simplicity we send the string null terminated from the client side
      res = preferences.putString(key, (const char*)command_ptr);
      break;
    case PT_BLOB:
      ets_printf("put bytes \n");
      res = preferences.putBytes(key, command_ptr, len);
      break;
    case PT_INVALID:
    default:
      ESP_LOGE(PREF_TAG, "Prefrences put invalid type");
      response[2] = 1;
      response[3] = 1;
      response[4] = 254;
      return 6;
  }

  response[2] = 1; // response nargs
  response[3] = 4;          // length of first parameter

  response[4] = (res >> 0)  & 0xff;
  response[5] = (res >> 8)  & 0xff;
  response[6] = (res >> 16) & 0xff;
  response[7] = (res >> 24) & 0xff;

  return 9;
}

int pref_get(const uint8_t command[], uint8_t response[])
{
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         key size                    < 1 byte >
  //[4..n]      key                         < n byte >
  //[n]         type                        < 1 byte >

  uint8_t nargs = command[2];
  char key[16];

  // command_ptr points to the next argument, in this case
  // it points to the length of key string
  const uint8_t* command_ptr = &command[3];

  // restricting the return as 32 bit integer as it is enough
  uint32_t res_size = 0;

  // all the kind of values can fit in a 64 bit integer
  uint32_t res=0;

  if(nargs != 2) {
    ESP_LOGE(PREF_TAG, "Prefrences put wrong number of arguments");
    response[2] = 1;
    response[3] = 0;
    return 5;
  }

  memset(key, 0x00, sizeof(key));
  memcpy(key, command_ptr+1, *command_ptr);
  key[*command_ptr] = '\0';

  // next argument
  command_ptr += *command_ptr + 1;

  command_ptr++; // The first byte contains the length of the parameter, which is 1
  PreferenceType type = static_cast<PreferenceType>(*command_ptr);

  command_ptr++;

  switch(type) {
    case PT_I8:
      res = static_cast<int8_t>(preferences.getChar(key));
      res_size = 1;
      break;
    case PT_U8:
      res = static_cast<uint8_t>(preferences.getUChar(key));
      res_size = 1;
      break;
    case PT_I16:
      res = static_cast<int16_t>(preferences.getShort(key));
      res_size = 2;
      break;
    case PT_U16:
      res_size = 2;
      res = static_cast<uint16_t>(preferences.getUShort(key));
      break;
    case PT_I32:
      res_size = 4;
      res = static_cast<int32_t>(preferences.getInt(key));
      break;
    case PT_U32:
      res_size = 4;
      res = static_cast<uint32_t>(preferences.getUInt(key));
      break;
    case PT_I64:
      res_size = 8;
      res = static_cast<int64_t>(preferences.getLong64(key));
      break;
    case PT_U64:
      res_size = 8;
      res = static_cast<uint64_t>(preferences.getULong64(key));
      break;
    case PT_STR:
      res_size = preferences.getString(key, (char*) &response[5], SPI_MAX_DMA_LEN - 8);
      goto array_return;
    case PT_BLOB:
      res_size = preferences.getBytes(key, &response[5], SPI_MAX_DMA_LEN - 8);
      goto array_return;
    case PT_INVALID:
    default:
      ESP_LOGE(PREF_TAG, "Prefrences put invalid type");
      response[2] = 1;
      response[3] = 0;
      return 5;
  }

  // fill the response buffer
  for(uint8_t i=0; i<res_size; i++) {
    response[5+i] = (res >> ((res_size-i-1) << 3)) & 0xff;
  }

array_return:

  response[2] = 1; // the number of parameters

  // the next 2 bytes are the size of the returned value. Since the client api support length with only 2 bytes
  // we can return string and blobs up to that size
  response[3] = (res_size >> 8)  & 0xff; // readParamLen16 wants little endian length
  response[4] = (res_size >> 0)  & 0xff;

  return 6 + res_size;
}

int pref_getType(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         key size                    < 1 byte >
  //[4..n]      key                         < n byte >

  uint8_t nargs = command[2];
  char key[16];

  // command_ptr points to the next argument, in this case
  // it points to the length of key string
  const uint8_t* command_ptr = &command[3];

  memset(key, 0x00, sizeof(key));
  memcpy(key, command_ptr+1, *command_ptr);
  key[*command_ptr] = '\0';


  response[2] = 1; // response nargs
  response[3] = 1; // response nargs
  response[4] = preferences.getType(key);

  return 6;
}

/*
 * BLE vHCI API
 */
#include "esp_bt.h"

#define TO_HOST_BUF_SIZE                256 // bytes
static RingbufHandle_t buf_handle       = NULL;
static SemaphoreHandle_t vhci_send_sem  = NULL;

static void controller_rcv_pkt_ready() {
	if (vhci_send_sem) {
		xSemaphoreGive(vhci_send_sem);
  }
}

/*
 * The following callback is called when the bt controller has some data available
 * this data is put into a queue that is then consumed by calling ble_read
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len) {
  if(buf_handle == NULL) {
    ets_printf("failed host_rcv_pkt\n");
    return ESP_FAIL;
  }

  UBaseType_t res = xRingbufferSend(buf_handle, data, len, pdMS_TO_TICKS(2000)); // TODO verify xTicksToWait value

  if (res != pdTRUE) {
    ets_printf("unable to send data to ring buffer\n");
  }
  return ESP_OK;
}

static esp_bt_controller_config_t btControllerConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
static esp_vhci_host_callback_t vhciHostCb = {
	controller_rcv_pkt_ready,
	host_rcv_pkt
};

int ble_begin(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >

  esp_err_t ret = ESP_OK;

  if((ret = esp_bt_controller_init(&btControllerConfig)) != ESP_OK) {
    ets_printf("failed esp_bt_controller_init %s\n", esp_err_to_name(ret));

    goto exit;
  }

  while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE);

  if((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
    ets_printf("failed esp_bt_controller_enable %s\n", esp_err_to_name(ret));

    goto exit;
  }

  if((buf_handle = xRingbufferCreate(TO_HOST_BUF_SIZE, RINGBUF_TYPE_BYTEBUF)) == NULL) {
    ret = ESP_ERR_NO_MEM;
    ets_printf("failed xRingbufferCreate\n");

    goto exit;
  }

  vhci_send_sem = xSemaphoreCreateBinary();
	if (vhci_send_sem == NULL) {
		ets_printf("Failed to create VHCI send sem\n");
		ret =  ESP_ERR_NO_MEM;
    goto exit;
	}
  xSemaphoreGive(vhci_send_sem);

  esp_bt_sleep_enable();

  esp_vhci_host_register_callback(&vhciHostCb);

exit:
  response[2] = 1;          // number of parameters
  response[3] = 1;          // length of first parameter
  response[4] = ret;

  return 6;
}

int ble_end(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >

	esp_bt_controller_disable();
	esp_bt_controller_deinit();

  if(buf_handle != NULL) {
    vRingbufferDelete(buf_handle);
  }

  if (vhci_send_sem != NULL) {
		/* Dummy take and give sema before deleting it */
		xSemaphoreTake(vhci_send_sem, pdMS_TO_TICKS(2000));
		xSemaphoreGive(vhci_send_sem);
		vSemaphoreDelete(vhci_send_sem);
		vhci_send_sem = NULL;
	}

  response[2] = 1;          // number of parameters
  response[3] = 1;          // length of first parameter
  response[4] = 1;

  return 6;
}

int ble_available(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  uint16_t available = 0;
  if(buf_handle != NULL) {
    available = TO_HOST_BUF_SIZE - xRingbufferGetCurFreeSize(buf_handle);
  }

  response[2] = 1;          // number of parameters
  response[3] = 2;          // length of first parameter
  response[4] = (available >> 8)  & 0xff;
  response[5] = (available >> 0)  & 0xff;

  return 7;
}

int ble_peek(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         the number 2                < 1 byte >
  //[4..5]      size                        < 2 byte >
  // this could be useless xQueuePeek
  uint8_t nargs = command[2];
  // if nargs != 1 -> error
  size_t res = 0;
  // uint16_t size = ntohs(*((uint16_t *) &command[4]));
  uint16_t size = *((uint16_t *) &command[4]);
  uint8_t* received = nullptr;

  if(size > TO_HOST_BUF_SIZE - xRingbufferGetCurFreeSize(buf_handle)) {
    size = 0;
    goto exit;
  }

  received = (uint8_t*)xRingbufferReceiveUpTo(buf_handle, &res, pdMS_TO_TICKS(2000), size);

  memcpy(&response[5], received, res);

exit:
  response[2] = 1;          // number of parameters
  response[3] = (size >> 8)  & 0xff;
  response[4] = (size >> 0)  & 0xff;

  return 6 + res;
}

int ble_read(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3]         the number 2                < 1 byte >
  //[4..5]      size                        < 2 byte >
  uint8_t nargs = command[2];
  // if nargs != 1 -> error
  size_t res = 0;
  // uint16_t size = ntohs(*((uint16_t *) &command[4]));
  uint16_t size = *((uint16_t *) &command[4]);
  uint8_t* received = nullptr;

  if(size > TO_HOST_BUF_SIZE - xRingbufferGetCurFreeSize(buf_handle)) {
    size = 0;
    goto exit;
  }

  received = (uint8_t*)xRingbufferReceiveUpTo(buf_handle, &res, pdMS_TO_TICKS(2000), size);

  memcpy(&response[5], received, res);

  vRingbufferReturnItem(buf_handle, received);

exit:
  response[2] = 1;          // number of parameters
  response[3] = (size >> 8)  & 0xff;
  response[4] = (size >> 0)  & 0xff;

  return 6 + res;
}

int ble_write(const uint8_t command[], uint8_t response[]) {
  //[0]         CMD_START                   < 0xE0   >
  //[1]         Command                     < 1 byte >
  //[2]         N args                      < 1 byte >
  //[3..4]      size                        < 2 byte >
  //[4..4+size] buffer                      < size byte >

  uint8_t nargs = command[2];
  // if nargs != 1 -> error

  uint16_t size = ntohs(*((uint16_t *) &command[3]));

  while(!esp_vhci_host_check_send_available()) { // TODO add timeout
    // TODO delay
  }

  if (vhci_send_sem && xSemaphoreTake(vhci_send_sem, pdMS_TO_TICKS(2000)) == pdTRUE) {
    esp_vhci_host_send_packet((uint8_t*)&command[5], size);
	}

  response[2] = 1;          // number of parameters
  response[3] = 2;          // length of first parameter
  response[4] = (size >> 0)  & 0xff;
  response[5] = (size >> 8)  & 0xff;

  return 7;
}

typedef int (*CommandHandlerType)(const uint8_t command[], uint8_t response[]);

const CommandHandlerType commandHandlers[] = {
  // 0x00 -> 0x0f
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

  // 0x10 -> 0x1f
  setNet, setPassPhrase, setKey, NULL, setIPconfig, setDNSconfig, setHostname, setPowerMode, setApNet, setApPassPhrase, setDebug, getTemperature, NULL, NULL, getDNSconfig, getReasonCode,

  // 0x20 -> 0x2f
  getConnStatus, getIPaddr, getMACaddr, getCurrSSID, getCurrBSSID, getCurrRSSI, getCurrEnct, scanNetworks, startServerTcp, getStateTcp, dataSentTcp, availDataTcp, getDataTcp, startClientTcp, stopClientTcp, getClientStateTcp,

  // 0x30 -> 0x3f
  disconnect, NULL, getIdxRSSI, getIdxEnct, reqHostByName, getHostByName, startScanNetworks, getFwVersion, NULL, sendUDPdata, getRemoteData, getTime, getIdxBSSID, getIdxChannel, ping, getSocket,

  // 0x40 -> 0x4f
  setEnt, NULL, NULL, NULL, sendDataTcp, getDataBufTcp, insertDataBuf, NULL, NULL, NULL,

  // BLE functions 0x4a -> 0x4f
  ble_begin,      // 0x4a
  ble_end,        // 0x4b
  ble_available,  // 0x4c
  ble_peek,       // 0x4d
  ble_read,       // 0x4e
  ble_write,      // 0x4f

  // 0x50 -> 0x54
  setPinMode, setDigitalWrite, setAnalogWrite, getDigitalRead, getAnalogRead,

  // KVStore functions 0x55 -> 0x87
  pref_begin,       // 0x55
  pref_end,         // 0x56
  pref_clear,       // 0x57
  pref_remove,      // 0x58
  pref_len,         // 0x59
  pref_stat,        // 0x5A
  pref_put,         // 0x5B
  pref_get,         // 0x5C
  pref_getType,     // 0x5D
  NULL, NULL,

  // 0x60 -> 0x6f
  writeFile, readFile, deleteFile, existsFile, downloadFile,  applyOTA, renameFile, downloadOTA, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

  // Low-level BSD-like sockets functions.
  // 0x70 -> 0x7f
  socket_socket,        // 0x70
  socket_close,         // 0x71
  socket_errno,         // 0x72
  socket_bind,          // 0x73
  socket_listen,        // 0x74
  socket_accept,        // 0x75
  socket_connect,       // 0x76
  socket_send,          // 0x77
  socket_recv,          // 0x78
  socket_sendto,        // 0x79
  socket_recvfrom,      // 0x7A
  socket_ioctl,         // 0x7B
  socket_poll,          // 0x7C
  socket_setsockopt,    // 0x7D
  socket_getsockopt,    // 0x7E
  socket_getpeername,   // 0x7F
};
#define NUM_COMMAND_HANDLERS (sizeof(commandHandlers) / sizeof(commandHandlers[0]))

CommandHandlerClass::CommandHandlerClass()
{
}

static const int GPIO_IRQ = 0;

void CommandHandlerClass::begin()
{
  pinMode(GPIO_IRQ, OUTPUT);

  for (int i = 0; i < MAX_SOCKETS; i++) {
    socketTypes[i] = 255;
  }

  _updateGpio0PinSemaphore = xSemaphoreCreateCounting(2, 0);

  WiFi.onReceive(CommandHandlerClass::onWiFiReceive);
  WiFi.onDisconnect(CommandHandlerClass::onWiFiDisconnect);

  xTaskCreatePinnedToCore(CommandHandlerClass::gpio0Updater, "gpio0Updater", 8192, NULL, 1, NULL, 1);
}

#define UDIV_UP(a, b) (((a) + (b) - 1) / (b))
#define ALIGN_UP(a, b) (UDIV_UP(a, b) * (b))

int CommandHandlerClass::handle(const uint8_t command[], uint8_t response[])
{
  int responseLength = 0;

  if (command[0] == 0xe0 && command[1] < NUM_COMMAND_HANDLERS) {
    CommandHandlerType commandHandlerType = commandHandlers[command[1]];

    if (commandHandlerType) {
      responseLength = commandHandlerType(command, response);
    }
  }

  if (responseLength == 0) {
    response[0] = 0xef;
    response[1] = 0x00;
    response[2] = 0xee;

    responseLength = 3;
  } else {
    response[0] = 0xe0;
    response[1] = (0x80 | command[1]);
    response[responseLength - 1] = 0xee;
  }

  xSemaphoreGive(_updateGpio0PinSemaphore);

  return ALIGN_UP(responseLength, 4);
}

void CommandHandlerClass::gpio0Updater(void*)
{
  while (1) {
    CommandHandler.updateGpio0Pin();
  }
}

void CommandHandlerClass::updateGpio0Pin()
{
  xSemaphoreTake(_updateGpio0PinSemaphore, portMAX_DELAY);

  int available = 0;

  for (int i = 0; i < MAX_SOCKETS; i++) {
    if (socketTypes[i] == 0x00) {
      if (tcpServers[i] && (tcpServers[i].hasClient() || tcpServers[i].available())) {
        available = 1;
        break;
      } else if (tcpClients[i] && tcpClients[i].connected() && tcpClients[i].available()) {
        available = 1;
        break;
      }
    }

    if (socketTypes[i] == 0x01 && udps[i] && (udps[i].available() || udps[i].parsePacket())) {
      available = 1;
      break;
    }

    if (socketTypes[i] == 0x02 && tlsClients[i] && tlsClients[i].connected() && tlsClients[i].available()) {
      available = 1;
      break;
    }

    if (socketTypes[i] == 0x04 && bearsslClient.connected() && bearsslClient.available()) {
      available = 1;
      break;
    }
  }

  if (available) {
    digitalWrite(GPIO_IRQ, HIGH);
  } else {
    digitalWrite(GPIO_IRQ, LOW);
  }

  vTaskDelay(1);
}

void CommandHandlerClass::onWiFiReceive()
{
  CommandHandler.handleWiFiReceive();
}

void CommandHandlerClass::handleWiFiReceive()
{
  xSemaphoreGiveFromISR(_updateGpio0PinSemaphore, NULL);
}

void CommandHandlerClass::onWiFiDisconnect()
{
  CommandHandler.handleWiFiDisconnect();
}

void CommandHandlerClass::handleWiFiDisconnect()
{
  // workaround to stop lwip_connect hanging
  // close all non-listening sockets

  for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++) {
    struct sockaddr_in addr;
    size_t addrLen = sizeof(addr);
    int socket = LWIP_SOCKET_OFFSET + i;

    if (lwip_getsockname(socket, (sockaddr*)&addr, &addrLen) < 0) {
      continue;
    }

    if (addr.sin_addr.s_addr != 0) {
      // non-listening socket, close
      close(socket);
    }
  }
}

CommandHandlerClass CommandHandler;
