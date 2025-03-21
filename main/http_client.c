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
  char *buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER);
  if (buffer == NULL) {
    return -1;
  }
  esp_http_client_config_t config = {
    .url = url,
    .timeout_ms = 20000,
  };

  spi_flash_mmap_handle_t handle;
  const unsigned char* certs_data = NULL;

  if(cert_pem != NULL) {
    config.cert_pem = cert_pem;
  } else {
    config.crt_bundle_attach = esp_crt_bundle_attach;

    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "certs");
    if (part == NULL) {
      return 0;
    }

    int ret = esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA, (const void**)&certs_data, &handle);
    if (ret != ESP_OK) {
      return 0;
    }

    ret = esp_crt_bundle_attach(&config);
    if (ret != ESP_OK) {
      return 0;
    }

    ret = esp_crt_bundle_set(certs_data, CRT_BUNDLE_SIZE);
    if (ret != ESP_OK) {
      return 0;
    }
  }

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
    ESP_LOGE(TAG, "esp_http_client_open failed: %d", err);
    free(buffer);
    return -1;
  }
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
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  free(buffer);

  return 0;
}