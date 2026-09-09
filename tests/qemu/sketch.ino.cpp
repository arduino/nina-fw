/*
 * Test-only entry point for exercising WiFiSSLClient under ESP32 QEMU.
 * This file is overlaid on main/sketch.ino.cpp in temporary source trees; it
 * is never part of a normal firmware build.
 */

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiSSLClient.h>

#include "QemuTestConfig.h"

#ifndef QEMU_TLS_ITERATIONS
#define QEMU_TLS_ITERATIONS 6000
#endif

#ifndef QEMU_TLS_WARMUP
#define QEMU_TLS_WARMUP 32
#endif

#ifndef QEMU_TLS_HOST
#define QEMU_TLS_HOST "127.0.0.1"
#endif

#ifndef QEMU_TLS_PORT
#define QEMU_TLS_PORT 9
#endif

#ifndef QEMU_TLS_TIMEOUT_MS
#define QEMU_TLS_TIMEOUT_MS 25
#endif

#ifdef QEMU_TLS_HTTPS
#ifndef QEMU_WIFI_SSID
#error "QEMU_WIFI_SSID is required for HTTPS mode"
#endif
#ifndef QEMU_WIFI_PASSWORD
#define QEMU_WIFI_PASSWORD ""
#endif
#ifndef QEMU_TLS_PATH
#define QEMU_TLS_PATH "/"
#endif
#endif

extern "C" esp_err_t nina_certificate_store_init(void) __attribute__((weak));

static WiFiSSLClient client;
static unsigned long attempt;
static unsigned long successes;
static unsigned long failures;
static size_t measuredStartHeap;
static size_t minimumHeap = SIZE_MAX;
static bool finished;

static void printSample(const char *kind, size_t freeHeap, size_t largestBlock)
{
  ets_printf(
    "QEMU_TLS,%s,attempt=%lu,successes=%lu,failures=%lu,free=%u,min=%u,largest=%u\n",
    kind, attempt, successes, failures, (unsigned int)freeHeap,
    (unsigned int)minimumHeap, (unsigned int)largestBlock);
}

#ifdef QEMU_TLS_HTTPS
static void connectWiFi()
{
  ets_printf("QEMU_TLS,WIFI_CONNECT,ssid=%s\n", QEMU_WIFI_SSID);
  WiFi.begin(QEMU_WIFI_SSID, QEMU_WIFI_PASSWORD);

  const unsigned long deadline = millis() + 30000;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    ets_printf("QEMU_TLS,SKIP,reason=wifi-association-timeout\n");
    finished = true;
  } else {
    ets_printf("QEMU_TLS,WIFI_CONNECTED\n");
  }
}
#endif

void setup()
{
  /* WiFi.status() initializes ESP-IDF networking even in loopback mode. */
  WiFi.status();

  if (nina_certificate_store_init != NULL) {
    const esp_err_t result = nina_certificate_store_init();
    if (result != ESP_OK) {
      ets_printf("QEMU_TLS,INFRA_ERROR,reason=certificate-store,error=%d\n", result);
      finished = true;
      return;
    }
  }

#ifdef QEMU_TLS_HTTPS
  connectWiFi();
#endif

  if (!finished) {
    ets_printf(
      "QEMU_TLS,START,iterations=%u,warmup=%u,host=%s,port=%u\n",
      QEMU_TLS_ITERATIONS, QEMU_TLS_WARMUP, QEMU_TLS_HOST, QEMU_TLS_PORT);
  }
}

void loop()
{
  if (finished) {
    delay(1000);
    return;
  }

  attempt++;
  client.setConnectionTimeout(QEMU_TLS_TIMEOUT_MS);

  if (client.connect(QEMU_TLS_HOST, QEMU_TLS_PORT)) {
    successes++;
#ifdef QEMU_TLS_HTTPS
    client.print("GET " QEMU_TLS_PATH " HTTP/1.1\r\nHost: " QEMU_TLS_HOST
                 "\r\nConnection: close\r\n\r\n");

    const unsigned long deadline = millis() + 10000;
    while (millis() < deadline && (client.connected() || client.available())) {
      while (client.available()) {
        client.read();
      }
    }
#endif
  } else {
    failures++;
  }
  client.stop();

  const size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  if (attempt == QEMU_TLS_WARMUP) {
    measuredStartHeap = freeHeap;
    minimumHeap = freeHeap;
    printSample("WARMUP_DONE", freeHeap, largestBlock);
  } else if (attempt > QEMU_TLS_WARMUP) {
    if (freeHeap < minimumHeap) {
      minimumHeap = freeHeap;
    }
    if ((attempt % 100) == 0) {
      printSample("SAMPLE", freeHeap, largestBlock);
    }
  }

  if (attempt >= QEMU_TLS_ITERATIONS) {
    const size_t loss = measuredStartHeap > freeHeap ? measuredStartHeap - freeHeap : 0;
    ets_printf(
      "QEMU_TLS,DONE,attempt=%lu,successes=%lu,failures=%lu,start=%u,free=%u,min=%u,largest=%u,loss=%u\n",
      attempt, successes, failures, (unsigned int)measuredStartHeap,
      (unsigned int)freeHeap, (unsigned int)minimumHeap,
      (unsigned int)largestBlock, (unsigned int)loss);
    finished = true;
  }
}
