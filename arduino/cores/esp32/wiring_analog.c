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
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <soc/adc_channel.h>

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

int analogRead(uint32_t pin, uint32_t atten)
{
  #define DEFAULT_VREF 1100
  static esp_adc_cal_characteristics_t *adc_chars;
  adc_channel_t channel;

  switch(pin)
  {
    case ADC1_CHANNEL_0_GPIO_NUM:
      channel = ADC1_GPIO36_CHANNEL;
      break;
    case ADC1_CHANNEL_1_GPIO_NUM:
      channel = ADC1_GPIO37_CHANNEL;
      break;
    case ADC1_CHANNEL_2_GPIO_NUM:
      channel = ADC1_GPIO38_CHANNEL;
      break;
    case ADC1_CHANNEL_3_GPIO_NUM:
      channel = ADC1_GPIO39_CHANNEL;
      break;
    case ADC1_CHANNEL_4_GPIO_NUM:
      channel = ADC1_GPIO32_CHANNEL;
      break;
    case ADC1_CHANNEL_5_GPIO_NUM:
      channel = ADC1_GPIO33_CHANNEL;
      break;
    case ADC1_CHANNEL_6_GPIO_NUM:
      channel = ADC1_GPIO34_CHANNEL;
      break;
    case ADC1_CHANNEL_7_GPIO_NUM:
      channel = ADC1_GPIO35_CHANNEL;
      break;
    default:
      return -1;
  }

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(channel, atten);

  adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

  int val = adc1_get_raw(channel);
  return val;
}
