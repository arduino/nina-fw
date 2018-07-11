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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define ARDUINO_MAIN
#include "Arduino.h"

void arduino_main(void*) {
  init();

  setup();

  while (1) {
    loop();
  }
}

extern "C" {
  void app_main() {
    xTaskCreatePinnedToCore(arduino_main, "arduino", 8192, NULL, 1, NULL, 1);
  }
}
