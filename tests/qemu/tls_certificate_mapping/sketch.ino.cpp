/* Verify that repeated TLS connections do not leak certificate mappings. */

#include <esp_heap_caps.h>
#include <esp_spi_flash.h>
#include <esp_system.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiSSLClient.h>

static const unsigned long ITERATIONS = 128;
static const unsigned long WARMUP = 8;
static const size_t MAXIMUM_HEAP_LOSS = 512;
static const char HOST[] = "127.0.0.1";
static const uint16_t PORT = 9;
static const unsigned long CONNECTION_TIMEOUT_MS = 25;

static WiFiSSLClient client;
static unsigned long attempt;
static unsigned long successes;
static unsigned long failures;
static size_t measuredStartHeap;
static size_t minimumHeap = SIZE_MAX;
static uint32_t measuredStartPages;
static uint32_t minimumPages = UINT32_MAX;
static bool finished;

static void printSample(const char *kind, size_t freeHeap, size_t largestBlock,
                        uint32_t freePages) {
  ets_printf("QEMU_TEST,%s,name=tls_certificate_mapping,attempt=%lu,connection_"
             "successes=%lu,connection_failures=%lu,free=%u,min=%u,largest=%u,"
             "free_pages=%u,min_pages=%u\n",
             kind, attempt, successes, failures, (unsigned int)freeHeap,
             (unsigned int)minimumHeap, (unsigned int)largestBlock,
             (unsigned int)freePages, (unsigned int)minimumPages);
}

void setup() {
  /* WiFi.status() initializes ESP-IDF networking for the connection path. */
  WiFi.status();

  ets_printf("QEMU_TEST,START,name=tls_certificate_mapping,iterations=%lu,"
             "warmup=%lu\n",
             ITERATIONS, WARMUP);
}

void loop() {
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
  const uint32_t freePages = spi_flash_mmap_get_free_pages(SPI_FLASH_MMAP_DATA);

  if (attempt == WARMUP) {
    measuredStartHeap = freeHeap;
    minimumHeap = freeHeap;
    measuredStartPages = freePages;
    minimumPages = freePages;
    printSample("WARMUP_DONE", freeHeap, largestBlock, freePages);
  } else if (attempt > WARMUP) {
    if (freeHeap < minimumHeap) {
      minimumHeap = freeHeap;
    }
    if (freePages < minimumPages) {
      minimumPages = freePages;
    }
    if ((attempt % 32) == 0) {
      printSample("SAMPLE", freeHeap, largestBlock, freePages);
    }
  }

  if (attempt >= ITERATIONS) {
    const size_t loss =
        measuredStartHeap > freeHeap ? measuredStartHeap - freeHeap : 0;
    const uint32_t pageLoss =
        measuredStartPages > freePages ? measuredStartPages - freePages : 0;
    const char *status =
        freePages == measuredStartPages && loss <= MAXIMUM_HEAP_LOSS ? "PASS"
                                                                     : "FAIL";
    ets_printf("QEMU_TEST,%s,name=tls_certificate_mapping,attempt=%lu,"
               "connection_successes=%lu,connection_failures=%lu,start=%u,free="
               "%u,min=%u,largest=%u,loss=%u,maximum_loss=%u,start_pages=%u,"
               "free_pages=%u,min_pages=%u,page_loss=%u\n",
               status, attempt, successes, failures,
               (unsigned int)measuredStartHeap, (unsigned int)freeHeap,
               (unsigned int)minimumHeap, (unsigned int)largestBlock,
               (unsigned int)loss, (unsigned int)MAXIMUM_HEAP_LOSS,
               (unsigned int)measuredStartPages, (unsigned int)freePages,
               (unsigned int)minimumPages, (unsigned int)pageLoss);
    finished = true;
  }
}
