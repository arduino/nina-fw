// esp_http_client.c
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include <stdio.h>

#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 	128

static const char* TAG = "HTTP_HANDLER";

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
        case HTTP_EVENT_ON_CONNECTED:
        case HTTP_EVENT_HEADER_SENT:
        case HTTP_EVENT_ON_FINISH:
        case HTTP_EVENT_DISCONNECTED:
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                //fwrite((char*)evt->data, sizeof(uint8_t), evt->data_len, (FILE*)evt->user_data);
            }
            break;
    }
    return ESP_OK;
}

int downloadAndSaveFile(char* url, char* filename, FILE* f) {

  char *buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER);
  if (buffer == NULL) {
    return -1;
  }
  esp_http_client_config_t config = {
    .url = url,
    .event_handler = _http_event_handler,
    .user_data = f,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
    free(buffer);
    return -1;
  }
  int content_length =  esp_http_client_fetch_headers(client);
  int total_read_len = 0, read_len;
  while (total_read_len < content_length) {
    read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
    fwrite(buffer, sizeof(uint8_t), read_len, f);
    if (read_len <= 0) {
      break;
    }
    total_read_len += read_len;
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  free(buffer);	

  return 0;
}