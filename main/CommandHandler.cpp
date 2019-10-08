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

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiSSLClient.h>
#include <WiFiUdp.h>
#include "esp_wpa2.h"

#include "CommandHandler.h"

#include "Arduino.h"

const char FIRMWARE_VERSION[6] = "1.4.0";

// AWS Device Certificate
// NOTE: I'm aware this certificate is here :)
char AWS_CERT_CRT[1300] = "-----BEGIN CERTIFICATE-----\n"\
"MIIDWTCCAkGgAwIBAgIUHi7YIHwvdKnUKTKE4MzqaVvVW7QwDQYJKoZIhvcNAQEL\n"\
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n"\
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTE5MDkyNTE2NDA1\n"\
"NVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n"\
"ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMygEW9cO1ZXQY4Fo3PY\n"\
"vBGV6WHwJYKIOd5iTZ4MQmkYNqn9q2YnuXEwYJ+sw6QxSYyZ9O8yniZfviggJ2Dg\n"\
"GdTGKIbSK7B/C3w6cLnwPNsKbA2xsxnQU3yoQ99noaue4kG+WL7a5SHJHwzcFpT4\n"\
"tVffsUlFtI9fTyGg75+0X4OJiKtzPhpVrCDesKDl0wLewqqgfxasgXWk3bLGCcBy\n"\
"7YPEM2x0lp6644xz0jkJ/3KO09+AuFG54K+zv7UZOi4Tph8eiKnI2/2sM58yC233\n"\
"pCnB8gtxCegvJJ1ByM5SR3Zw5C1hq6cgN5ePv1fQ7QqOnIHygc0gDp8/nw5gnH8P\n"\
"3LcCAwEAAaNgMF4wHwYDVR0jBBgwFoAU1YI5dEJDKJgyKP6e/lSezmki1tUwHQYD\n"\
"VR0OBBYEFDTH23PCBu1Pw4xdOR3rY3Pcueh4MAwGA1UdEwEB/wQCMAAwDgYDVR0P\n"\
"AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQA1p78t3Tk+6V5h0SlokRaC5bVm\n"\
"RoXwXRmmCsZJlwvIG25buBdUAWC/2odreV4anM9HmRnECxZMIV7Q0NiuVcl3Kiok\n"\
"xtWsdsCyZkH0OMcBuiTEu+o3osTtxAp8dkzcBlh768htDXZCsAzRjFTwtZ78BqFk\n"\
"rzduv1FDtpbxoD95X8B3MOc+ZrsZ5TTA+dpepeid6K3jmG9LPmFnahCkK31Hp5dv\n"\
"WKKDKZn51PvOVAvti1QeAYcFabgeXFWb8OuCJcqWEKFJuvQRvKrpyLfpSR4NNq7M\n"\
"nM12jsbhjrGYVCmQjczqOMqF+LMnXYUSY+o6gsBCM5XRAwOLY4S7Gv53K4+l\n"\
"-----END CERTIFICATE-----\n";

// AWS Device Private Key
// NOTE: I'm aware this certificate is here :)
char AWS_CERT_PRIVATE[1700] =
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


/*IPAddress*/uint32_t resolvedHostname;

#define MAX_SOCKETS CONFIG_LWIP_MAX_SOCKETS
uint8_t socketTypes[MAX_SOCKETS];
WiFiClient tcpClients[MAX_SOCKETS];
WiFiUDP udps[MAX_SOCKETS];
WiFiSSLClient tlsClients[MAX_SOCKETS];
WiFiServer tcpServers[MAX_SOCKETS];


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

  if (type == 0x00) {
    tcpServers[socket] = WiFiServer(port);

    tcpServers[socket].begin();

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
      WiFiClient client = tcpServers[socket].available();

      available = 255;

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
    } else {
      available = tcpClients[socket].available();
    }
  } else if (socketTypes[socket] == 0x01) {
    available = udps[socket].available();

    if (available <= 0) {
      available = udps[socket].parsePacket();
    }
  } else if (socketTypes[socket] == 0x02) {
    available = tlsClients[socket].available();
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
  }

  return 6;
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

    ets_printf("*** Commandhandler L551, .connect init'd\n");
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
    ets_printf("*** Commandhandler 595, .connect init'd\n");
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

    socketTypes[socket] = 255;
  } else if (socketTypes[socket] == 0x01) {
    udps[socket].stop();

    socketTypes[socket] = 255;
  } else if (socketTypes[socket] == 0x02) {
    tlsClients[socket].stop();

    socketTypes[socket] = 255;
  }

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

int wpa2EntSetIdentity(const uint8_t command[], uint8_t response[]) {
  char identity[32 + 1];

  memset(identity, 0x00, sizeof(identity));
  memcpy(identity, &command[4], command[3]);

  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)identity, strlen(identity));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetUsername(const uint8_t command[], uint8_t response[]) {
  char username[32 + 1];

  memset(username, 0x00, sizeof(username));
  memcpy(username, &command[4], command[3]);

  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetPassword(const uint8_t command[], uint8_t response[]) {
  char password[32 + 1];

  memset(password, 0x00, sizeof(password));
  memcpy(password, &command[4], command[3]);

  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int wpa2EntSetCACert(const uint8_t command[], uint8_t response[]) {
  // not yet implemented (need to decide if writing in the filesystem is better than loading every time)
  // keep in mind size limit for messages
  return 0;
}

int wpa2EntSetCertKey(const uint8_t command[], uint8_t response[]) {
  // not yet implemented (need to decide if writing in the filesystem is better than loading every time)
  // keep in mind size limit for messages
  return 0;
}

int wpa2EntEnable(const uint8_t command[], uint8_t response[]) {

  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
  esp_wifi_sta_wpa2_ent_enable(&config);

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

int setClientCert(const uint8_t command[], uint8_t response[]){
  ets_printf("*** Called setClientCert\n");
  ets_printf("\nFree internal heap: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  //size_t ca_cert_buf_size = (command[3] << 8 | command[4]);
  //char* ca_cert_buf = (char*)malloc(ca_cert_buf_size+1);

  
  //ets_printf("\nCert Sz: %d\n", sizeof(AWS_CERT_CRT));
  //memset(cert_buf, 0x00, sizeof(cert_buf));
  //memcpy(cert_buf, &command[4], sizeof(cert_buf));
  //ets_printf("\nCert: \n %s", cert_buf);
  // todo: add statement for allocation failing.

  ets_printf("\nFree internal heap: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  tlsClients[0].setCertificate(AWS_CERT_CRT);
  ets_printf("\nFree internal heap: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));

  
  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}


int setCertKey(const uint8_t command[], uint8_t response[]){
  ets_printf("*** Called setCertKey\n");

  ets_printf("\nFree internal heap: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  tlsClients[0].setPrivateKey(AWS_CERT_PRIVATE);
  ets_printf("\nFree internal heap: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));

  response[2] = 1; // number of parameters
  response[3] = 1; // parameter 1 length
  response[4] = 1;

  return 6;
}

typedef int (*CommandHandlerType)(const uint8_t command[], uint8_t response[]);

const CommandHandlerType commandHandlers[] = {
  // 0x00 -> 0x0f
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

  // 0x10 -> 0x1f
  setNet, setPassPhrase, setKey, NULL, setIPconfig, setDNSconfig, setHostname, setPowerMode, setApNet, setApPassPhrase, setDebug, getTemperature, NULL, NULL, NULL, NULL,

  // 0x20 -> 0x2f
  getConnStatus, getIPaddr, getMACaddr, getCurrSSID, getCurrBSSID, getCurrRSSI, getCurrEnct, scanNetworks, startServerTcp, getStateTcp, dataSentTcp, availDataTcp, getDataTcp, startClientTcp, stopClientTcp, getClientStateTcp,

  // 0x30 -> 0x3f
  disconnect, NULL, getIdxRSSI, getIdxEnct, reqHostByName, getHostByName, startScanNetworks, getFwVersion, NULL, sendUDPdata, getRemoteData, getTime, getIdxBSSID, getIdxChannel, ping, getSocket,

  // 0x40 -> 0x4f
  setClientCert, setCertKey, NULL, NULL, sendDataTcp, getDataBufTcp, insertDataBuf, NULL, NULL, NULL, wpa2EntSetIdentity, wpa2EntSetUsername, wpa2EntSetPassword, wpa2EntSetCACert, wpa2EntSetCertKey, wpa2EntEnable,

  // 0x50 -> 0x5f
  setPinMode, setDigitalWrite, setAnalogWrite,
};

#define NUM_COMMAND_HANDLERS (sizeof(commandHandlers) / sizeof(commandHandlers[0]))

CommandHandlerClass::CommandHandlerClass()
{
}

void CommandHandlerClass::begin()
{
  pinMode(0, OUTPUT);

  for (int i = 0; i < MAX_SOCKETS; i++) {
    socketTypes[i] = 255;
  }

  _updateGpio0PinSemaphore = xSemaphoreCreateCounting(2, 0);

  WiFi.onReceive(CommandHandlerClass::onWiFiReceive);

  xTaskCreatePinnedToCore(CommandHandlerClass::gpio0Updater, "gpio0Updater", 8192, NULL, 1, NULL, 1);
}

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

  return responseLength;
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
      if (tcpServers[i] && tcpServers[i].available()) {
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
  }

  if (available) {
    digitalWrite(0, HIGH);
  } else {
    digitalWrite(0, LOW);
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

CommandHandlerClass CommandHandler;
