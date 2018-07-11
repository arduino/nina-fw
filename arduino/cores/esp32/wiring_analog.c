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

#include <driver/ledc.h>

#include "wiring_analog.h"

void analogWrite(uint32_t pin, uint32_t value)
{
  periph_module_enable(PERIPH_LEDC_MODULE);

  ledc_timer_config_t timerConf = {
    .bit_num = LEDC_TIMER_10_BIT,
    .freq_hz = 1000,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num = (pin / 7),
  };
  ledc_timer_config(&timerConf);

  ledc_channel_config_t ledc_conf = {
    .channel = (pin % 7),
    .duty = (value << 2),
    .gpio_num = pin,
    .intr_type = LEDC_INTR_DISABLE,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_sel = (pin / 7)
  };
  ledc_channel_config(&ledc_conf);
}
