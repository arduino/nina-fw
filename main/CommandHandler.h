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

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdint.h>

class CommandHandlerClass {
public:
  CommandHandlerClass();

  void begin();
  int handle(const uint8_t command[], uint8_t response[]);

private:
  static void gpio0Updater(void*);
  void updateGpio0Pin();

  static void onWiFiReceive();
  void handleWiFiReceive();

  static void onWiFiDisconnect();
  void handleWiFiDisconnect();

private:
  SemaphoreHandle_t _updateGpio0PinSemaphore;
};

extern CommandHandlerClass CommandHandler;

extern "C" int downloadAndSaveFile(char * url, FILE * f, const char * cert_pem);

#endif
