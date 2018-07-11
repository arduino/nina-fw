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

#include <string.h>

#include <driver/gpio.h>
#include <driver/spi_slave.h>

#include "wiring_digital.h"
#include "WInterrupts.h"

#include "SPIS.h"

SPISClass::SPISClass(spi_host_device_t hostDevice, int dmaChannel, int mosiPin, int misoPin, int sclkPin, int csPin, int readyPin) :
  _hostDevice(hostDevice),
  _dmaChannel(dmaChannel),
  _mosiPin(mosiPin),
  _misoPin(misoPin),
  _sclkPin(sclkPin),
  _csPin(csPin),
  _readyPin(readyPin)
{
}

int SPISClass::begin()
{
  spi_bus_config_t busCfg;
  spi_slave_interface_config_t slvCfg;

  pinMode(_readyPin, OUTPUT);
  digitalWrite(_readyPin, HIGH);

  _readySemaphore = xSemaphoreCreateCounting(1, 0);

  attachInterrupt(_csPin, onChipSelect, FALLING);

  memset(&busCfg, 0x00, sizeof(busCfg));
  busCfg.mosi_io_num = _mosiPin;
  busCfg.miso_io_num = _misoPin;
  busCfg.sclk_io_num = _sclkPin;

  memset(&slvCfg, 0x00, sizeof(slvCfg));
  slvCfg.mode = 0;
  slvCfg.spics_io_num = _csPin;
  slvCfg.queue_size = 1;
  slvCfg.flags = 0;
  slvCfg.post_setup_cb = SPISClass::onSetupComplete;
  slvCfg.post_trans_cb = NULL;

  gpio_set_pull_mode((gpio_num_t)_mosiPin, GPIO_FLOATING);
  gpio_set_pull_mode((gpio_num_t)_sclkPin, GPIO_PULLDOWN_ONLY);
  gpio_set_pull_mode((gpio_num_t)_csPin,   GPIO_PULLUP_ONLY);

  spi_slave_initialize(_hostDevice, &busCfg, &slvCfg, _dmaChannel);

  return 1;
}

int SPISClass::transfer(uint8_t out[], uint8_t in[], size_t len)
{
  spi_slave_transaction_t slvTrans;
  spi_slave_transaction_t* slvRetTrans;

  memset(&slvTrans, 0x00, sizeof(slvTrans));

  slvTrans.length = len * 8;
  slvTrans.trans_len = 0;
  slvTrans.tx_buffer = out;
  slvTrans.rx_buffer = in;

  spi_slave_queue_trans(_hostDevice, &slvTrans, portMAX_DELAY);
  xSemaphoreTake(_readySemaphore, portMAX_DELAY);
  digitalWrite(_readyPin, LOW);
  spi_slave_get_trans_result(_hostDevice, &slvRetTrans, portMAX_DELAY);
  digitalWrite(_readyPin, HIGH);

  return (slvTrans.trans_len / 8);
}

void SPISClass::onChipSelect()
{
  SPIS.handleOnChipSelect();
}

void SPISClass::handleOnChipSelect()
{
  digitalWrite(_readyPin, HIGH);
}

void SPISClass::onSetupComplete(spi_slave_transaction_t*)
{
  SPIS.handleSetupComplete();
}

void SPISClass::handleSetupComplete()
{
  xSemaphoreGiveFromISR(_readySemaphore, NULL);
}

SPISClass SPIS(VSPI_HOST, 1, 12, 23, 18, 5, 33);
