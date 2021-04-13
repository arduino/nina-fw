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

#ifndef WIRING_DIGITAL_H
#define WIRING_DIGITAL_H

#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOW  0x00
#define HIGH 0x01

#define INPUT        0x00
#define OUTPUT       0x01
#define INPUT_PULLUP 0x02

extern void pinMode(uint32_t pin, uint32_t mode);

extern void digitalWrite(uint32_t pin, uint32_t val);

extern int digitalRead(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif // WIRING_DIGITAL_H
