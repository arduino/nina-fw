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

#ifndef RGBLED_H
#define RGBLED_H

#include <esp_event_loop.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <lwip/netif.h>

#include <Arduino.h>

class RGBClass
{
public:
  RGBClass();

  TaskHandle_t RGBtask_handle = NULL;
  TaskHandle_t RGBstop_handle = NULL;
  TaskHandle_t RGBrestart_handle = NULL;

  void init(void);

  void RGBmode(uint32_t mode);
  void RGBcolor(uint8_t red, uint8_t green, uint8_t blue);
  void clearLed(void);
  void ledRGBEvent(system_event_t* event);
  static void RGBstop(void*);
  static void RGBrestart(void*);
  static void APconnection(void*);
  static void APdisconnection(void*);
};

static RGBClass RGB;

#endif
