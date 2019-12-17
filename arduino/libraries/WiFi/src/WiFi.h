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

#ifndef WIFI_H
#define WIFI_H

#include <esp_event_loop.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <lwip/netif.h>

#include <Arduino.h>

typedef enum {
  WL_NO_SHIELD = 255,
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL,
  WL_SCAN_COMPLETED,
  WL_CONNECTED,
  WL_CONNECT_FAILED,
  WL_CONNECTION_LOST,
  WL_DISCONNECTED,
  WL_AP_LISTENING,
  WL_AP_CONNECTED,
  WL_AP_FAILED,
} wl_status_t;

#define MAX_SCAN_RESULTS 10
#define HOSTNAME_MAX_LENGTH 32

class WiFiClass
{
public:
  WiFiClass();

  uint8_t begin(const char* ssid);
  uint8_t begin(const char* ssid, uint8_t key_idx, const char* key);
  uint8_t begin(const char* ssid, const char* key);

  uint8_t beginAP(const char *ssid, uint8_t channel);
  uint8_t beginAP(const char *ssid, uint8_t key_idx, const char* key, uint8_t channel);
  uint8_t beginAP(const char *ssid, const char* key, uint8_t channel);

  uint8_t beginEnterprise(const char* ssid, const char* username, const char* password, const char* identity, const char* rootCA);
  uint8_t beginEnterpriseTLS(const char* ssid, const char* cert, const char* key, const char* identity, const char* rootCA);

  void config(/*IPAddress*/uint32_t local_ip, /*IPAddress*/uint32_t gateway, /*IPAddress*/uint32_t subnet);

  void setDNS(/*IPAddress*/uint32_t dns_server1, /*IPAddress*/uint32_t dns_server2);

  void hostname(const char* name);

  void disconnect();
  void end();

  uint8_t* macAddress(uint8_t* mac);

  uint32_t localIP();
  uint32_t subnetMask();
  uint32_t gatewayIP();
  char* SSID();
  int32_t RSSI();
  uint8_t encryptionType();
  uint8_t* BSSID(uint8_t* bssid);
  int8_t scanNetworks();
  char* SSID(uint8_t pos);
  int32_t RSSI(uint8_t pos);
  uint8_t encryptionType(uint8_t pos);
  uint8_t* BSSID(uint8_t pos, uint8_t* bssid);
  uint8_t channel(uint8_t pos);

  uint8_t status();
  uint8_t reasonCode();

  int hostByName(const char* hostname, /*IPAddress*/uint32_t& result);

  int ping(/*IPAddress*/uint32_t host, uint8_t ttl);

  unsigned long getTime();

  void lowPowerMode();
  void noLowPowerMode();

  void onReceive(void(*)(void));
  void onDisconnect(void(*)(void));

private:
  void init();

  static esp_err_t systemEventHandler(void* ctx, system_event_t* event);
  void handleSystemEvent(system_event_t* event);

  static err_t staNetifInputHandler(struct pbuf* p, struct netif* inp);
  static err_t apNetifInputHandler(struct pbuf* p, struct netif* inp);
  err_t handleStaNetifInput(struct pbuf* p, struct netif* inp);
  err_t handleApNetifInput(struct pbuf* p, struct netif* inp);

private:
  bool _initialized;
  volatile uint8_t _status;
  volatile uint8_t _reasonCode;
  EventGroupHandle_t _eventGroup;
  esp_interface_t _interface;

  wifi_ap_record_t _scanResults[MAX_SCAN_RESULTS];
  wifi_ap_record_t _apRecord;
  bool _staticIp;
  tcpip_adapter_ip_info_t _ipInfo;
  uint32_t _dnsServers[2];
  char _hostname[HOSTNAME_MAX_LENGTH+1];
  netif_input_fn _staNetifInput;
  netif_input_fn _apNetifInput;

  void (*_onReceiveCallback)(void);
  void (*_onDisconnectCallback)(void);

  char* _wpa2Cert;
  char* _wpa2Key;
  char* _wpa2RootCA;
};

extern WiFiClass WiFi;

#endif // WIFI_H
