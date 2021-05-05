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

void pinMode(uint32_t pin, uint32_t mode)
{
  switch (mode) {
    case INPUT:
      gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
      gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
      break;

    case OUTPUT:
      gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
      gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
      break;

    case INPUT_PULLUP:
      gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
      gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
      break;
  }

  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
}

void digitalWrite(uint32_t pin, uint32_t val)
{
  gpio_set_level((gpio_num_t)pin, val);
}

int digitalRead(uint32_t pin)
{
  return gpio_get_level(pin);
}
