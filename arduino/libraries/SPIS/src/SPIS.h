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

#ifndef SPIS_H
#define SPIS_H

#include <driver/spi_common.h>
#include <driver/spi_slave.h>

class SPISClass {

  public:
    SPISClass(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin, int readyPin);

    int begin();
    int transfer(uint8_t out[], uint8_t in[], size_t len);

  private:
    static void onChipSelect();
    void handleOnChipSelect();

    static void onSetupComplete(spi_slave_transaction_t*);
    void handleSetupComplete();

  private:
    spi_host_device_t _hostDevice;
    int _dmaChannel;
    int _mosiPin;
    int _misoPin;
    int _sclkPin;
    int _csPin;
    int _readyPin;

    intr_handle_t _csIntrHandle;

    SemaphoreHandle_t _readySemaphore;
};

extern SPISClass SPIS;

#endif

