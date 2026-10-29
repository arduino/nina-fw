/*
  This file is part of the Arduino NINA firmware.
  Copyright (c) 2026 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
*/

#ifndef CERTIFICATE_STORE_H
#define CERTIFICATE_STORE_H

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NINA_CERTIFICATE_STORE_UNAVAILABLE = 0,
  NINA_CERTIFICATE_STORE_BUNDLE,
  NINA_CERTIFICATE_STORE_PEM,
} nina_certificate_store_format_t;

/*
 * Map and validate the certificate partition. The successful mapping is
 * deliberately retained for the lifetime of the firmware because ESP-IDF's
 * certificate bundle keeps a pointer into it.
 *
 * Initialization is thread-safe and idempotent. Certificate consumers call
 * this before accessing the store.
 */
esp_err_t nina_certificate_store_init(void);

nina_certificate_store_format_t nina_certificate_store_format(void);
const unsigned char *nina_certificate_store_data(void);
size_t nina_certificate_store_size(void);

#ifdef __cplusplus
}
#endif

#endif /* CERTIFICATE_STORE_H */
