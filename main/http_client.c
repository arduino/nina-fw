// esp_http_client.c
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include <stdio.h>

#include "esp_http_client.h"
#include "esp_partition.h"
#include "esp_crt_bundle.h"

#define MAX_HTTP_RECV_BUFFER 	128

static const char* TAG = "HTTP_CLIENT";

int downloadAndSaveFile(char * url, FILE * f, const char * cert_pem)
{
  int result = -1;
  bool partition_mapped = false;
  bool client_open = false;
  char *buffer = NULL;
  esp_http_client_handle_t client = NULL;
  esp_http_client_config_t config = {
    .url = url,
    .timeout_ms = 20000,
  };

  spi_flash_mmap_handle_t handle;
  const unsigned char* certs_data = NULL;

  if(cert_pem != NULL) {
    config.cert_pem = cert_pem;
  } else {
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "certs");
    if (part == NULL) {
      ESP_LOGE(TAG, "certificate partition not found");
      goto cleanup;
    }

    int ret = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, (const void**)&certs_data, &handle);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "certificate partition mapping failed: %d", ret);
      goto cleanup;
    }
    partition_mapped = true;

    /* Starting from firmware 3.x.x cert partition starts at address 0xA000.
     * The arduino-fw-uploader tool uploads user certificates in PEM format to a
     * fixed flash address 0x10000. If you change again the partition table make
     * sure 0x10000 is in the range of your certs partition.
     * To make fimrware 3.x.x compatible with arduino-fw-uploader we first check
     * for PEM certificate at address 0x10000, if no valid certificate is found
     * we try to load the default certificate bundle from address 0xA000
     */
    const bool hasPEM = (memcmp(&certs_data[0x6000], "-----BEGIN CERTIFICATE-----", 26) == 0);

    if (hasPEM) {
      config.cert_pem = (const char*)&certs_data[0x6000];
    } else {
      config.crt_bundle_attach = esp_crt_bundle_attach;

      ret = esp_crt_bundle_attach(&config);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "certificate bundle attach failed: %d", ret);
        goto cleanup;
      }

      ret = esp_crt_bundle_set(certs_data, CRT_BUNDLE_SIZE);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "certificate bundle setup failed: %d", ret);
        goto cleanup;
      }
    }
  }

  buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER);
  if (buffer == NULL) {
    ESP_LOGE(TAG, "receive buffer allocation failed");
    goto cleanup;
  }

  client = esp_http_client_init(&config);
  if (client == NULL) {
    ESP_LOGE(TAG, "esp_http_client_init failed");
    goto cleanup;
  }
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "esp_http_client_open failed: %d", err);
    goto cleanup;
  }
  client_open = true;
  int content_length = esp_http_client_fetch_headers(client);
  int total_read_len = 0, read_len;
  while (total_read_len < content_length) {
    read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
    fwrite(buffer, sizeof(uint8_t), read_len, f);
    if (read_len <= 0) {
      break;
    }
    total_read_len += read_len;
    ESP_LOGV(TAG, "esp_http_client_read data received: %d, total %d", read_len, total_read_len);
  }
  ESP_LOGV(TAG, "connection closed, cleaning up, total %d bytes received", total_read_len);
  result = 0;

cleanup:
  if (client_open) {
    esp_http_client_close(client);
  }
  if (client != NULL) {
    esp_http_client_cleanup(client);
  }
  if (partition_mapped) {
    spi_flash_munmap(handle);
  }
  free(buffer);
  return result;
}
