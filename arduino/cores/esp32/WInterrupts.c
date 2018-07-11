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

#include <driver/gpio.h>

#include "wiring_digital.h"

#include "WInterrupts.h"

static voidFuncPtr callbacksInt[GPIO_NUM_MAX] = { NULL };

void IRAM_ATTR gpioInterruptHandler(void* arg)
{
  uint32_t pin = (uint32_t)arg;

  if (callbacksInt[pin]) {
    callbacksInt[pin]();
  }
}

void attachInterrupt(uint32_t pin, voidFuncPtr callback, uint32_t mode)
{
  callbacksInt[pin] = callback;

  switch (mode) {
    case LOW:
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL);
      gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL);
      break;

    case HIGH:
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_HIGH_LEVEL);
      gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_HIGH_LEVEL);
      break;

    case CHANGE:
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_ANYEDGE);
      break;

    case FALLING:
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_NEGEDGE);
      break;

    case RISING:
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_POSEDGE);
      break;

    default:
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
      break;
  }

  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);

  gpio_isr_handler_add((gpio_num_t)pin, gpioInterruptHandler, (void*)pin);

  gpio_intr_enable((gpio_num_t)pin);
}
