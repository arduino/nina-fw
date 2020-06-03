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

#include <time.h>

#include <esp_wifi.h>
#include <esp_wpa2.h>
#include <tcpip_adapter.h>

#include <lwip/apps/sntp.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/sockets.h>
#include <lwip/ip_addr.h>
#include <lwip/inet_chksum.h>

#include "WiFi.h"

WiFiClass::WiFiClass() :
  _initialized(false),
  _status(WL_NO_SHIELD),
  _reasonCode(0),
  _interface(ESP_IF_WIFI_STA),
  _onReceiveCallback(NULL),
  _onDisconnectCallback(NULL),
  _wpa2Cert(NULL),
  _wpa2Key(NULL),
  _wpa2RootCA(NULL)
{
  _eventGroup = xEventGroupCreate();
  memset(&_apRecord, 0x00, sizeof(_apRecord));
  _staticIp = false;
  memset(&_ipInfo, 0x00, sizeof(_ipInfo));
  memset(&_dnsServers, 0x00, sizeof(_dnsServers));
  memset(&_hostname, 0x00, sizeof(_hostname));
}

uint8_t WiFiClass::status()
{
  if (!_initialized) {
    _initialized = true;
    init();
  }

  return _status;
}

uint8_t WiFiClass::reasonCode()
{
  return _reasonCode;
}

int WiFiClass::hostByName(const char* hostname, /*IPAddress*/uint32_t& result)
{
  struct addrinfo hints;
  struct addrinfo* addr_list;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = 0;
  hints.ai_protocol = 0;

  result = 0xffffffff;

  if (getaddrinfo(hostname, NULL, &hints, &addr_list) != 0) {
    return 0;
  }

  result = ((struct sockaddr_in*)addr_list->ai_addr)->sin_addr.s_addr;

  freeaddrinfo(addr_list);

  return 1;
}

int WiFiClass::ping(/*IPAddress*/uint32_t host, uint8_t ttl)
{
  uint32_t timeout = 5000;

  int s = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);

  struct timeval timeoutVal;
  timeoutVal.tv_sec = (timeout / 1000);
  timeoutVal.tv_usec = (timeout % 1000) * 1000;

  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal));
  setsockopt(s, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

  struct __attribute__((__packed__)) {
    struct icmp_echo_hdr header;
    uint8_t data[32];
  } request;

  ICMPH_TYPE_SET(&request.header, ICMP_ECHO);
  ICMPH_CODE_SET(&request.header, 0);
  request.header.chksum = 0;
  request.header.id = 0xAFAF;
  request.header.seqno = random(0xffff);

  for (size_t i = 0; i < sizeof(request.data); i++) {
    request.data[i] = i;
  }

  request.header.chksum = inet_chksum(&request, sizeof(request));

  ip_addr_t addr;
  addr.type = IPADDR_TYPE_V4;
  addr.u_addr.ip4.addr = host;
  //  IP_ADDR4(&addr, ip[0], ip[1], ip[2], ip[3]);

  struct sockaddr_in to;
  struct sockaddr_in from;

  to.sin_len = sizeof(to);
  to.sin_family = AF_INET;
  inet_addr_from_ip4addr(&to.sin_addr, ip_2_ip4(&addr));

  sendto(s, &request, sizeof(request), 0, (struct sockaddr*)&to, sizeof(to));
  unsigned long sendTime = millis();
  unsigned long recvTime = 0;

  do {
    socklen_t fromlen = sizeof(from);

    struct __attribute__((__packed__)) {
      struct ip_hdr ipHeader;
      struct icmp_echo_hdr header;
    } response;

    int rxSize = recvfrom(s, &response, sizeof(response), 0, (struct sockaddr*)&from, (socklen_t*)&fromlen);
    if (rxSize == -1) {
      // time out
      break;
    }

    if (rxSize < sizeof(response)) {
      // too short
      continue;
    }

    if (from.sin_family != AF_INET) {
      // not IPv4
      continue;
    }

    if ((response.header.id == request.header.id) && (response.header.seqno == request.header.seqno)) {
      recvTime = millis();
    }
  } while (recvTime == 0);

  close(s);

  if (recvTime == 0) {
    return -1;
  } else {
    return (recvTime - sendTime);
  }
}

uint8_t WiFiClass::begin(const char* ssid)
{
  return begin(ssid, "");
}

uint8_t WiFiClass::begin(const char* ssid, uint8_t key_idx, const char* key)
{
  return begin(ssid, key);
}

uint8_t WiFiClass::begin(const char* ssid, const char* key)
{
  wifi_config_t wifiConfig;

  memset(&wifiConfig, 0x00, sizeof(wifiConfig));
  strncpy((char*)wifiConfig.sta.ssid, ssid, sizeof(wifiConfig.sta.ssid));
  strncpy((char*)wifiConfig.sta.password, key, sizeof(wifiConfig.sta.password));
  wifiConfig.sta.scan_method = WIFI_FAST_SCAN;
  _status = WL_NO_SSID_AVAIL;

  _interface = ESP_IF_WIFI_STA;

  xEventGroupClearBits(_eventGroup, BIT0);
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  xEventGroupWaitBits(_eventGroup, BIT0, false, true, portMAX_DELAY);

  if (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifiConfig) != ESP_OK) {
    _status = WL_CONNECT_FAILED;
  }

  if (_staticIp) {
    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &_ipInfo);
  }

  esp_wifi_connect();

  return _status;
}

uint8_t WiFiClass::beginAP(const char *ssid, uint8_t channel)
{
  wifi_config_t wifiConfig;

  memset(&wifiConfig, 0x00, sizeof(wifiConfig));
  strncpy((char*)wifiConfig.ap.ssid, ssid, sizeof(wifiConfig.sta.ssid));
  wifiConfig.ap.channel = channel;
  wifiConfig.ap.authmode = WIFI_AUTH_OPEN;
  wifiConfig.ap.max_connection = 4;

  _status = WL_NO_SSID_AVAIL;

  _interface = ESP_IF_WIFI_AP;

  xEventGroupClearBits(_eventGroup, BIT1);
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_AP);

  if (esp_wifi_set_config(ESP_IF_WIFI_AP, &wifiConfig) != ESP_OK) {
    _status = WL_AP_FAILED;
  } else {
    esp_wifi_start();
    xEventGroupWaitBits(_eventGroup, BIT1, false, true, portMAX_DELAY);
  }

  return _status;
}

uint8_t WiFiClass::beginAP(const char *ssid, uint8_t key_idx, const char* key, uint8_t channel)
{
  wifi_config_t wifiConfig;

  memset(&wifiConfig, 0x00, sizeof(wifiConfig));
  strncpy((char*)wifiConfig.ap.ssid, ssid, sizeof(wifiConfig.sta.ssid));
  strncpy((char*)wifiConfig.ap.password, key, sizeof(wifiConfig.sta.password));
  wifiConfig.ap.channel = channel;
  wifiConfig.ap.authmode = WIFI_AUTH_WEP;
  wifiConfig.ap.max_connection = 4;

  _status = WL_NO_SSID_AVAIL;

  _interface = ESP_IF_WIFI_AP;

  xEventGroupClearBits(_eventGroup, BIT1);
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_AP);

  if (esp_wifi_set_config(ESP_IF_WIFI_AP, &wifiConfig) != ESP_OK) {
    _status = WL_AP_FAILED;
  } else {
    esp_wifi_start();
    xEventGroupWaitBits(_eventGroup, BIT1, false, true, portMAX_DELAY);
  }

  return _status;
}

uint8_t WiFiClass::beginAP(const char *ssid, const char* key, uint8_t channel)
{
  wifi_config_t wifiConfig;

  memset(&wifiConfig, 0x00, sizeof(wifiConfig));
  strncpy((char*)wifiConfig.ap.ssid, ssid, sizeof(wifiConfig.sta.ssid));
  strncpy((char*)wifiConfig.ap.password, key, sizeof(wifiConfig.sta.password));
  wifiConfig.ap.channel = channel;
  wifiConfig.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  wifiConfig.ap.max_connection = 4;

  _status = WL_NO_SSID_AVAIL;

  _interface = ESP_IF_WIFI_AP;

  xEventGroupClearBits(_eventGroup, BIT1);
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_AP);

  if (esp_wifi_set_config(ESP_IF_WIFI_AP, &wifiConfig) != ESP_OK) {
    _status = WL_AP_FAILED;
  } else {
    esp_wifi_start();
    xEventGroupWaitBits(_eventGroup, BIT1, false, true, portMAX_DELAY);
  }

  return _status;
}

uint8_t WiFiClass::beginEnterprise(const char* ssid, const char* username, const char* password, const char* identity, const char* rootCA)
{
  esp_wifi_sta_wpa2_ent_clear_username();
  esp_wifi_sta_wpa2_ent_clear_password();
  esp_wifi_sta_wpa2_ent_clear_identity();
  esp_wifi_sta_wpa2_ent_clear_ca_cert();
  esp_wifi_sta_wpa2_ent_clear_cert_key();

  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();

  esp_wifi_sta_wpa2_ent_enable(&config);

  int usernameLen = strlen(username);
  if (usernameLen) {
    esp_wifi_sta_wpa2_ent_set_username((const unsigned char*)username, usernameLen);
  }

  int passwordLen = strlen(password);
  if (passwordLen) {
    esp_wifi_sta_wpa2_ent_set_password((const unsigned char*)password, passwordLen);
  }

  int identityLen = strlen(identity);
  if (identityLen) {
    esp_wifi_sta_wpa2_ent_set_identity((const unsigned char*)identity, identityLen);
  }

  int rootCALen = strlen(rootCA);
  if (rootCALen) {
    _wpa2RootCA = (char*)realloc(_wpa2RootCA, rootCALen + 1);
    memcpy(_wpa2RootCA, rootCA, rootCALen + 1);

    esp_wifi_sta_wpa2_ent_set_ca_cert((const unsigned char*)_wpa2RootCA, rootCALen + 1);
  }

  return begin(ssid);
}

uint8_t WiFiClass::beginEnterpriseTLS(const char* ssid, const char* cert, const char* key, const char* identity, const char* rootCA)
{
  esp_wifi_sta_wpa2_ent_clear_username();
  esp_wifi_sta_wpa2_ent_clear_password();
  esp_wifi_sta_wpa2_ent_clear_identity();
  esp_wifi_sta_wpa2_ent_clear_ca_cert();
  esp_wifi_sta_wpa2_ent_clear_cert_key();

  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();

  esp_wifi_sta_wpa2_ent_enable(&config);

  int certLen = strlen(cert);
  int keyLen = strlen(key);

  _wpa2Cert = (char*)realloc(_wpa2Cert, certLen + 1);
  memcpy(_wpa2Cert, cert, certLen + 1);
  _wpa2Key = (char*)realloc(_wpa2Key, keyLen + 1);
  memcpy(_wpa2Key, key, keyLen + 1);

  esp_wifi_sta_wpa2_ent_set_cert_key((const unsigned char*)_wpa2Cert, certLen + 1, (const unsigned char*)_wpa2Key, keyLen + 1, NULL, 0);

  int identityLen = strlen(identity);
  if (identityLen) {
    esp_wifi_sta_wpa2_ent_set_identity((const unsigned char*)identity, identityLen);
  }

  int rootCALen = strlen(rootCA);
  if (rootCALen) {
    _wpa2RootCA = (char*)realloc(_wpa2RootCA, rootCALen + 1);
    memcpy(_wpa2RootCA, rootCA, rootCALen + 1);

    esp_wifi_sta_wpa2_ent_set_ca_cert((const unsigned char*)_wpa2RootCA, rootCALen + 1);
  }

  return begin(ssid);
}

void WiFiClass::config(/*IPAddress*/uint32_t local_ip, /*IPAddress*/uint32_t gateway, /*IPAddress*/uint32_t subnet)
{
  dns_clear_servers(true);

  _staticIp = true;
  _ipInfo.ip.addr = local_ip;
  _ipInfo.gw.addr = gateway;
  _ipInfo.netmask.addr = subnet;

  if (_interface == ESP_IF_WIFI_AP) {
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &_ipInfo);
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
  } else {
    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &_ipInfo);
  }
}

void WiFiClass::setDNS(/*IPAddress*/uint32_t dns_server1, /*IPAddress*/uint32_t dns_server2)
{
  ip_addr_t d;
  d.type = IPADDR_TYPE_V4;

  _dnsServers[0] = dns_server1;
  _dnsServers[1] = dns_server2;

  if (dns_server1) {
    d.u_addr.ip4.addr = static_cast<uint32_t>(dns_server1);
    dns_setserver(0, &d);
  }

  if (dns_server2) {
    d.u_addr.ip4.addr = static_cast<uint32_t>(dns_server2);
    dns_setserver(1, &d);
  }
}

void WiFiClass::hostname(const char* name)
{
  strncpy(_hostname, name, HOSTNAME_MAX_LENGTH);
}

void WiFiClass::disconnect()
{
  esp_wifi_disconnect();
  esp_wifi_stop();
}

void WiFiClass::end()
{
  esp_wifi_stop();
}

uint8_t* WiFiClass::macAddress(uint8_t* mac)
{
  uint8_t macTemp[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  esp_wifi_get_mac(_interface, macTemp);

  mac[0] = macTemp[5];
  mac[1] = macTemp[4];
  mac[2] = macTemp[3];
  mac[3] = macTemp[2];
  mac[4] = macTemp[1];
  mac[5] = macTemp[0];

  return mac;
}

uint32_t WiFiClass::localIP()
{
  return _ipInfo.ip.addr;
}

uint32_t WiFiClass::subnetMask()
{
  return _ipInfo.netmask.addr;
}

uint32_t WiFiClass::gatewayIP()
{
  return _ipInfo.gw.addr;
}

char* WiFiClass::SSID()
{
  return (char*)_apRecord.ssid;
}

int32_t WiFiClass::RSSI()
{
  if (_interface == ESP_IF_WIFI_AP) {
    return 0;
  } else {
    esp_wifi_sta_get_ap_info(&_apRecord);

    return _apRecord.rssi;
  }
}

uint8_t WiFiClass::encryptionType()
{
  uint8_t encryptionType = _apRecord.authmode;

  if (encryptionType == WIFI_AUTH_OPEN) {
    encryptionType = 7;
  } else if (encryptionType == WIFI_AUTH_WEP) {
    encryptionType = 5;
  } else if (encryptionType == WIFI_AUTH_WPA_PSK) {
    encryptionType = 2;
  } else if (encryptionType == WIFI_AUTH_WPA2_PSK || encryptionType == WIFI_AUTH_WPA_WPA2_PSK) {
    encryptionType = 4;
  } else {
    // unknown?
    encryptionType = 255;
  }

  return encryptionType;
}

uint8_t* WiFiClass::BSSID(uint8_t* bssid)
{
  if (_interface == ESP_IF_WIFI_AP) {
    return macAddress(bssid);
  } else {
    bssid[0] = _apRecord.bssid[5];
    bssid[1] = _apRecord.bssid[4];
    bssid[2] = _apRecord.bssid[3];
    bssid[3] = _apRecord.bssid[2];
    bssid[4] = _apRecord.bssid[1];
    bssid[5] = _apRecord.bssid[0];

    return bssid;
  }
}

int8_t WiFiClass::scanNetworks()
{
  xEventGroupClearBits(_eventGroup, BIT0);
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  xEventGroupWaitBits(_eventGroup, BIT0, false, true, portMAX_DELAY);

  wifi_scan_config_t config;

  config.ssid = 0;
  config.bssid = 0;
  config.channel = 0;
  config.show_hidden = false;
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min = 100;
  config.scan_time.active.max = 300;

  xEventGroupClearBits(_eventGroup, BIT2);

  if (esp_wifi_scan_start(&config, false) != ESP_OK) {
    _status = WL_NO_SSID_AVAIL;
    return 0;
  }

  xEventGroupWaitBits(_eventGroup, BIT2, false, true, portMAX_DELAY);

  uint16_t numNetworks;
  esp_wifi_scan_get_ap_num(&numNetworks);

  if (numNetworks > MAX_SCAN_RESULTS) {
    numNetworks = MAX_SCAN_RESULTS;
  }

  esp_wifi_scan_get_ap_records(&numNetworks, _scanResults);

  _status = WL_SCAN_COMPLETED;

  return numNetworks;
}

char* WiFiClass::SSID(uint8_t pos)
{
  return (char*)_scanResults[pos].ssid;
}

int32_t WiFiClass::RSSI(uint8_t pos)
{
  return _scanResults[pos].rssi;
}

uint8_t WiFiClass::encryptionType(uint8_t pos)
{
  uint8_t encryptionType = _scanResults[pos].authmode;

  if (encryptionType == WIFI_AUTH_OPEN) {
    encryptionType = 7;
  } else if (encryptionType == WIFI_AUTH_WEP) {
    encryptionType = 5;
  } else if (encryptionType == WIFI_AUTH_WPA_PSK) {
    encryptionType = 2;
  } else if (encryptionType == WIFI_AUTH_WPA2_PSK || encryptionType == WIFI_AUTH_WPA_WPA2_PSK) {
    encryptionType = 4;
  } else {
    // unknown?
    encryptionType = 255;
  }

  return encryptionType;
}

uint8_t* WiFiClass::BSSID(uint8_t pos, uint8_t* bssid)
{
  const uint8_t* tempBssid = _scanResults[pos].bssid;

  bssid[0] = tempBssid[5];
  bssid[1] = tempBssid[4];
  bssid[2] = tempBssid[3];
  bssid[3] = tempBssid[2];
  bssid[4] = tempBssid[1];
  bssid[5] = tempBssid[0];

  return bssid;
}

uint8_t WiFiClass::channel(uint8_t pos)
{
  return _scanResults[pos].primary;
}

unsigned long WiFiClass::getTime()
{
  time_t now;

  time(&now);

  if (now < 946684800) {
    return 0;
  }

  return now;
}

void WiFiClass::lowPowerMode()
{
  esp_wifi_set_ps(WIFI_PS_MODEM);
}

void WiFiClass::noLowPowerMode()
{
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void WiFiClass::onReceive(void(*callback)(void))
{
  _onReceiveCallback = callback;
}

void WiFiClass::onDisconnect(void(*callback)(void))
{
  _onDisconnectCallback = callback;
}

err_t WiFiClass::staNetifInputHandler(struct pbuf* p, struct netif* inp)
{
  return WiFi.handleStaNetifInput(p, inp);
}

err_t WiFiClass::apNetifInputHandler(struct pbuf* p, struct netif* inp)
{
  return WiFi.handleApNetifInput(p, inp);
}

err_t WiFiClass::handleStaNetifInput(struct pbuf* p, struct netif* inp)
{
  err_t result = _staNetifInput(p, inp);

  if (_onReceiveCallback) {
    _onReceiveCallback();
  }

  return result;
}

err_t WiFiClass::handleApNetifInput(struct pbuf* p, struct netif* inp)
{
  err_t result = _apNetifInput(p, inp);

  if (_onReceiveCallback) {
    _onReceiveCallback();
  }

  return result;
}

void WiFiClass::init()
{
  tcpip_adapter_init();
  esp_event_loop_init(WiFiClass::systemEventHandler, this);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char*)"0.pool.ntp.org");
  sntp_setservername(1, (char*)"1.pool.ntp.org");
  sntp_setservername(2, (char*)"2.pool.ntp.org");
  sntp_init();
  _status = WL_IDLE_STATUS;
}

esp_err_t WiFiClass::systemEventHandler(void* ctx, system_event_t* event)
{
  ((WiFiClass*)ctx)->handleSystemEvent(event);

  return ESP_OK;
}

void WiFiClass::handleSystemEvent(system_event_t* event)
{
  switch (event->event_id) {
    case SYSTEM_EVENT_SCAN_DONE:
      xEventGroupSetBits(_eventGroup, BIT2);
      break;

    case SYSTEM_EVENT_STA_START: {
      struct netif* staNetif;
      if (strlen(_hostname) == 0) {
        uint8_t mac[6];
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
        sprintf(_hostname, "arduino-%.2x%.2x", mac[4], mac[5]);
      }
      tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, _hostname);

      if (tcpip_adapter_get_netif(TCPIP_ADAPTER_IF_STA, (void**)&staNetif) == ESP_OK) {
        if (staNetif->input != WiFiClass::staNetifInputHandler) {
          _staNetifInput = staNetif->input;

          staNetif->input = WiFiClass::staNetifInputHandler;
        }
      }

      xEventGroupSetBits(_eventGroup, BIT0);
      break;
    }

    case SYSTEM_EVENT_STA_STOP:
      xEventGroupClearBits(_eventGroup, BIT0);
      break;

    case SYSTEM_EVENT_STA_CONNECTED:
      _reasonCode = 0;

      esp_wifi_sta_get_ap_info(&_apRecord);

      if (_staticIp) {
        // re-apply the custom DNS settings
        setDNS(_dnsServers[0], _dnsServers[1]);

        // static IP
        _status = WL_CONNECTED;
      }
      break;

    case SYSTEM_EVENT_STA_GOT_IP:
      memcpy(&_ipInfo, &event->event_info.got_ip.ip_info, sizeof(_ipInfo));
      _status = WL_CONNECTED;
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED: {
      uint8_t reason = event->event_info.disconnected.reason;

      _reasonCode = reason;

      memset(&_apRecord, 0x00, sizeof(_apRecord));

      if (reason == 201/*NO_AP_FOUND*/ || reason == 202/*AUTH_FAIL*/) {
        _status = WL_CONNECT_FAILED;
      } else if (reason == 203/*ASSOC_FAIL*/) {
        // try to reconnect
        esp_wifi_connect();
      } else {
        _status = WL_DISCONNECTED;

        if (_onDisconnectCallback) {
          _onDisconnectCallback();
        }
      }
      break;
    }

    case SYSTEM_EVENT_STA_LOST_IP:
      memset(&_ipInfo, 0x00, sizeof(_ipInfo));
      memset(&_dnsServers, 0x00, sizeof(_dnsServers));
      _status = WL_CONNECTION_LOST;
      break;

    case SYSTEM_EVENT_AP_START: {
      struct netif* apNetif;

      if (tcpip_adapter_get_netif(TCPIP_ADAPTER_IF_AP, (void**)&apNetif) == ESP_OK) {
        if (apNetif->input != WiFiClass::apNetifInputHandler) {
          _apNetifInput = apNetif->input;

          apNetif->input = WiFiClass::apNetifInputHandler;
        }
      }

      wifi_config_t config;

      esp_wifi_get_config(ESP_IF_WIFI_AP, &config);
      memcpy(_apRecord.ssid, config.ap.ssid, sizeof(config.ap.ssid));
      _apRecord.authmode = config.ap.authmode;

      if (_staticIp) {
        // custom static IP
        tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
        tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &_ipInfo);
        tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);

        // re-apply the custom DNS settings
        setDNS(_dnsServers[0], _dnsServers[1]);
      } else {
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &_ipInfo);
      }

      _status = WL_AP_LISTENING;
      xEventGroupSetBits(_eventGroup, BIT1);
      break;
    }

    case SYSTEM_EVENT_AP_STOP:
      _status = WL_IDLE_STATUS;
      memset(&_apRecord, 0x00, sizeof(_apRecord));
      memset(&_ipInfo, 0x00, sizeof(_ipInfo));
      xEventGroupClearBits(_eventGroup, BIT1);
      break;

    case SYSTEM_EVENT_AP_STACONNECTED:
      _status = WL_AP_CONNECTED;
      break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
      wifi_sta_list_t staList;

      esp_wifi_ap_get_sta_list(&staList);

      if (staList.num == 0) {
        _status = WL_AP_LISTENING;
      }
      break;

    default:
      break;
  }
}

WiFiClass WiFi;
