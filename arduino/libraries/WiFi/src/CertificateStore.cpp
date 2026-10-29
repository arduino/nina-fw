/*
  This file is part of the Arduino NINA firmware.
  Copyright (c) 2026 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_crt_bundle.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"

#include "CertificateStore.h"

static const size_t PEM_OFFSET = 0x6000;
static const char PEM_HEADER[] = "-----BEGIN CERTIFICATE-----";

typedef enum {
  INITIALIZATION_PENDING,
  INITIALIZATION_IN_PROGRESS,
  INITIALIZATION_COMPLETE,
} initialization_state_t;

static portMUX_TYPE initializationMutex = portMUX_INITIALIZER_UNLOCKED;
static initialization_state_t initializationState = INITIALIZATION_PENDING;
static esp_err_t initializationResult = ESP_ERR_INVALID_STATE;
static nina_certificate_store_format_t format = NINA_CERTIFICATE_STORE_UNAVAILABLE;
static const unsigned char *certificateData;
static size_t certificateSize;

/* Retain ownership of the mapping for as long as certificateData is in use. */
static spi_flash_mmap_handle_t certificateMappingHandle;

static esp_err_t initializeCertificateStore(void)
{
  const esp_partition_t *partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "certs");
  if (partition == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  if (partition->size < CRT_BUNDLE_SIZE ||
      partition->size < PEM_OFFSET + sizeof(PEM_HEADER) - 1) {
    return ESP_ERR_INVALID_SIZE;
  }

  const unsigned char *mappedData = NULL;
  esp_err_t result = esp_partition_mmap(
    partition, 0, partition->size, SPI_FLASH_MMAP_DATA,
    (const void **)&mappedData, &certificateMappingHandle);
  if (result != ESP_OK) {
    return result;
  }

  if (memcmp(mappedData + PEM_OFFSET, PEM_HEADER, sizeof(PEM_HEADER) - 1) == 0) {
    const unsigned char *pem = mappedData + PEM_OFFSET;
    const size_t maximumPemSize = partition->size - PEM_OFFSET;
    const unsigned char *terminator =
      (const unsigned char *)memchr(pem, '\0', maximumPemSize);
    if (terminator == NULL) {
      spi_flash_munmap(certificateMappingHandle);
      return ESP_ERR_INVALID_SIZE;
    }

    certificateData = pem;
    certificateSize = (size_t)(terminator - pem) + 1;
    format = NINA_CERTIFICATE_STORE_PEM;
  } else {
    result = esp_crt_bundle_set(mappedData, CRT_BUNDLE_SIZE);
    if (result != ESP_OK) {
      spi_flash_munmap(certificateMappingHandle);
      return result;
    }

    certificateData = mappedData;
    certificateSize = CRT_BUNDLE_SIZE;
    format = NINA_CERTIFICATE_STORE_BUNDLE;
  }

  return ESP_OK;
}

esp_err_t nina_certificate_store_init(void)
{
  bool initialize = false;

  while (!initialize) {
    portENTER_CRITICAL(&initializationMutex);
    if (initializationState == INITIALIZATION_PENDING) {
      initializationState = INITIALIZATION_IN_PROGRESS;
      initialize = true;
    } else if (initializationState == INITIALIZATION_COMPLETE) {
      const esp_err_t result = initializationResult;
      portEXIT_CRITICAL(&initializationMutex);
      return result;
    }
    portEXIT_CRITICAL(&initializationMutex);

    if (!initialize) {
      vTaskDelay(1);
    }
  }

  const esp_err_t result = initializeCertificateStore();

  portENTER_CRITICAL(&initializationMutex);
  initializationResult = result;
  initializationState = INITIALIZATION_COMPLETE;
  portEXIT_CRITICAL(&initializationMutex);

  return result;
}

nina_certificate_store_format_t nina_certificate_store_format(void)
{
  return format;
}

const unsigned char *nina_certificate_store_data(void)
{
  return certificateData;
}

size_t nina_certificate_store_size(void)
{
  return certificateSize;
}
