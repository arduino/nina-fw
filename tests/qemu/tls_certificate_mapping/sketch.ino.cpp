/* Verify that repeated TLS connections do not leak certificate mappings. */

#include <esp_heap_caps.h>
#include <esp_system.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiSSLClient.h>

#include "CertificateStore.h"

static const unsigned long ITERATIONS = 6000;
static const unsigned long WARMUP = 32;
static const size_t MAXIMUM_HEAP_LOSS = 4096;
static const char HOST[] = "127.0.0.1";
static const uint16_t PORT = 9;
static const unsigned long CONNECTION_TIMEOUT_MS = 25;

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
    "QEMU_TEST,%s,name=tls_certificate_mapping,attempt=%lu,successes=%lu,failures=%lu,free=%u,min=%u,largest=%u\n",
    kind, attempt, successes, failures, (unsigned int)freeHeap,
    (unsigned int)minimumHeap, (unsigned int)largestBlock);
}

void setup()
{
  /* WiFi.status() initializes ESP-IDF networking for the connection path. */
  WiFi.status();

  const esp_err_t result = nina_certificate_store_init();
  if (result != ESP_OK) {
    ets_printf(
      "QEMU_TEST,FAIL,name=tls_certificate_mapping,reason=certificate-store,error=%d\n",
      result);
    finished = true;
    return;
  }

  ets_printf(
    "QEMU_TEST,START,name=tls_certificate_mapping,iterations=%lu,warmup=%lu\n",
    ITERATIONS, WARMUP);
}

void loop()
{
  if (finished) {
    delay(1000);
    return;
  }

  attempt++;
  client.setConnectionTimeout(CONNECTION_TIMEOUT_MS);
  if (client.connect(HOST, PORT)) {
    successes++;
  } else {
    failures++;
  }
  client.stop();

  const size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  if (attempt == WARMUP) {
    measuredStartHeap = freeHeap;
    minimumHeap = freeHeap;
    printSample("WARMUP_DONE", freeHeap, largestBlock);
  } else if (attempt > WARMUP) {
    if (freeHeap < minimumHeap) {
      minimumHeap = freeHeap;
    }
    if ((attempt % 100) == 0) {
      printSample("SAMPLE", freeHeap, largestBlock);
    }
  }

  if (attempt >= ITERATIONS) {
    const size_t loss = measuredStartHeap > freeHeap ? measuredStartHeap - freeHeap : 0;
    const char *status = loss <= MAXIMUM_HEAP_LOSS ? "PASS" : "FAIL";
    ets_printf(
      "QEMU_TEST,%s,name=tls_certificate_mapping,attempt=%lu,successes=%lu,failures=%lu,start=%u,free=%u,min=%u,largest=%u,loss=%u,maximum_loss=%u\n",
      status, attempt, successes, failures, (unsigned int)measuredStartHeap,
      (unsigned int)freeHeap, (unsigned int)minimumHeap,
      (unsigned int)largestBlock, (unsigned int)loss,
      (unsigned int)MAXIMUM_HEAP_LOSS);
    finished = true;
  }
}
