// esp_http_client.c
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include <stdio.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "CertificateStore.h"

#define MAX_HTTP_RECV_BUFFER 	128

static const char* TAG = "HTTP_CLIENT";

int downloadAndSaveFile(char * url, FILE * f, const char * cert_pem)
{
  int result = -1;
  esp_http_client_handle_t client = NULL;
  char *buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER);
  if (buffer == NULL) {
    return -1;
  }
  esp_http_client_config_t config = {
    .url = url,
    .timeout_ms = 20000,
  };

  if(cert_pem != NULL) {
    config.cert_pem = cert_pem;
  } else {
    if (nina_certificate_store_init() != ESP_OK) {
      goto cleanup;
    }

    const nina_certificate_store_format_t certificateFormat = nina_certificate_store_format();
    if (certificateFormat == NINA_CERTIFICATE_STORE_PEM) {
      config.cert_pem = (const char *)nina_certificate_store_data();
    } else if (certificateFormat == NINA_CERTIFICATE_STORE_BUNDLE) {
      config.crt_bundle_attach = esp_crt_bundle_attach;
    } else {
      goto cleanup;
    }
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
  int content_length = esp_http_client_fetch_headers(client);
  int total_read_len = 0, read_len;
  while (total_read_len < content_length) {
    read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
    if (read_len <= 0) {
      break;
    }
    fwrite(buffer, sizeof(uint8_t), read_len, f);
    total_read_len += read_len;
    ESP_LOGV(TAG, "esp_http_client_read data received: %d, total %d", read_len, total_read_len);
  }
  ESP_LOGV(TAG, "connection closed, cleaning up, total %d bytes received", total_read_len);
  result = 0;

cleanup:
  if (client != NULL) {
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
  }
  free(buffer);

  return result;
}
