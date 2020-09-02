/*
 * TWI/I2C library for Arduino Zero
 * Copyright (c) 2015 Arduino LLC. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

extern "C" {
#include <string.h>

#include <driver/gpio.h>
#include <esp_intr.h>
#include <rom/ets_sys.h>
#include <soc/gpio_sig_map.h>
}

#include <Arduino.h>

#include "Wire.h"

#define ETS_I2C0_INUM  0
#define ETS_I2C1_INUM  1

TwoWire::TwoWire(periph_module_t peripheralModule, i2c_dev_t* i2cDev, uint8_t pinSDA, uint8_t pinSCL)
{
  this->_peripheral = peripheralModule;
  this->_dev = i2cDev;
  this->_uc_pinSDA = pinSDA;
  this->_uc_pinSCL = pinSCL;
  transmissionBegun = false;
}

void TwoWire::begin(void) {
  //Master Mode
  periph_module_enable(_peripheral);

  setClock(TWI_CLOCK);

  _dev->ctr.val = 0;
  _dev->ctr.ms_mode = 1;
  _dev->ctr.sda_force_out = 1;
  _dev->ctr.scl_force_out = 1;
  _dev->ctr.clk_en = 1;

  _dev->fifo_conf.tx_fifo_empty_thrhd = 0;

  _dev->timeout.tout = 5000;
  _dev->fifo_conf.nonfifo_en = 0;

  _dev->slave_addr.addr = 0;
  _dev->slave_addr.en_10bit = 0;

  gpio_config_t gpioConf;

  gpioConf.intr_type = GPIO_INTR_DISABLE;
  gpioConf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  gpioConf.pin_bit_mask = (1 << _uc_pinSCL) | (1 << _uc_pinSDA);
  gpioConf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConf.pull_up_en = GPIO_PULLUP_ENABLE;

  gpio_config(&gpioConf);    

  if (_peripheral == PERIPH_I2C0_MODULE) {
    gpio_matrix_out(_uc_pinSCL, I2CEXT0_SCL_OUT_IDX, 0,  0);
    gpio_matrix_out(_uc_pinSDA, I2CEXT0_SDA_OUT_IDX, 0,  0);
    gpio_matrix_in(_uc_pinSCL, I2CEXT0_SCL_IN_IDX, 0);
    gpio_matrix_in(_uc_pinSDA, I2CEXT0_SDA_IN_IDX, 0);
  } else if (_peripheral == PERIPH_I2C1_MODULE) {
    gpio_matrix_out(_uc_pinSCL, I2CEXT1_SCL_OUT_IDX, 0,  0);
    gpio_matrix_out(_uc_pinSDA, I2CEXT1_SDA_OUT_IDX, 0,  0);
    gpio_matrix_in(_uc_pinSCL, I2CEXT1_SCL_IN_IDX, 0);
    gpio_matrix_in(_uc_pinSDA, I2CEXT1_SDA_IN_IDX, 0);
  }
}

void TwoWire::begin(uint8_t address) {
  //Slave mode

  periph_module_enable(_peripheral);

  setClock(TWI_CLOCK);

  _dev->ctr.val = 0;
  _dev->ctr.ms_mode = 0;
  _dev->ctr.sda_force_out = 1;
  _dev->ctr.scl_force_out = 1;
  _dev->ctr.clk_en = 1;

  _dev->fifo_conf.tx_fifo_empty_thrhd = 0;
  _dev->fifo_conf.rx_fifo_full_thrhd = 0x1f;

  _dev->timeout.tout = 50000;
  _dev->fifo_conf.nonfifo_en = 0;

  _dev->slave_addr.addr = (address);
  _dev->slave_addr.en_10bit = 0;

  gpio_config_t gpioConf;

  gpioConf.intr_type = GPIO_INTR_DISABLE;
  gpioConf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  gpioConf.pin_bit_mask = (1 << _uc_pinSCL) | (1 << _uc_pinSDA);
  gpioConf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConf.pull_up_en = GPIO_PULLUP_ENABLE;

  gpio_config(&gpioConf);    

  if (_peripheral == PERIPH_I2C0_MODULE) {
    gpio_matrix_out(_uc_pinSCL, I2CEXT0_SCL_OUT_IDX, 0,  0);
    gpio_matrix_out(_uc_pinSDA, I2CEXT0_SDA_OUT_IDX, 0,  0);
    gpio_matrix_in(_uc_pinSCL, I2CEXT0_SCL_IN_IDX, 0);
    gpio_matrix_in(_uc_pinSDA, I2CEXT0_SDA_IN_IDX, 0);
  } else if (_peripheral == PERIPH_I2C1_MODULE) {
    gpio_matrix_out(_uc_pinSCL, I2CEXT1_SCL_OUT_IDX, 0,  0);
    gpio_matrix_out(_uc_pinSDA, I2CEXT1_SDA_OUT_IDX, 0,  0);
    gpio_matrix_in(_uc_pinSCL, I2CEXT1_SCL_IN_IDX, 0);
    gpio_matrix_in(_uc_pinSDA, I2CEXT1_SDA_IN_IDX, 0);
  }

  if (_peripheral == PERIPH_I2C0_MODULE) {
    ESP_INTR_DISABLE(ETS_I2C0_INUM);
    intr_matrix_set(0, ETS_I2C_EXT0_INTR_SOURCE, ETS_I2C0_INUM);
    xt_set_interrupt_handler(ETS_I2C_EXT0_INTR_SOURCE, TwoWire::onService, this);
    ESP_INTR_ENABLE(ETS_I2C0_INUM);
  } else if (_peripheral == PERIPH_I2C1_MODULE) {
    ESP_INTR_DISABLE(ETS_I2C1_INUM);
    intr_matrix_set(0, ETS_I2C_EXT1_INTR_SOURCE, ETS_I2C1_INUM);
    xt_set_interrupt_handler(ETS_I2C1_INUM, TwoWire::onService, this);
    ESP_INTR_ENABLE(ETS_I2C1_INUM);
  }

  _dev->fifo_conf.tx_fifo_rst = 1;
  _dev->fifo_conf.tx_fifo_rst = 0;
  _dev->fifo_conf.rx_fifo_rst = 1;
  _dev->fifo_conf.rx_fifo_rst = 0;

  _dev->int_clr.val = 0xFFFFFFFF;
  _dev->int_ena.val = 0;
  _dev->int_ena.trans_complete = 1;
  _dev->int_ena.slave_tran_comp = 1;
  _dev->int_ena.rx_fifo_full = 1;
}

void TwoWire::setClock(uint32_t baudrate) {
  uint32_t period = (APB_CLK_FREQ / baudrate) / 2;

  _dev->scl_low_period.period = period;
  _dev->scl_high_period.period = period;

  _dev->scl_start_hold.time = 50;
  _dev->scl_rstart_setup.time = 50;

  _dev->scl_stop_hold.time = 50;
  _dev->scl_stop_setup.time = 50;

  _dev->sda_hold.time = 50;
  _dev->sda_sample.time = 50;
}

void TwoWire::end() {
  _dev->int_ena.val = 0;

  if (_peripheral == PERIPH_I2C0_MODULE) {
    ESP_INTR_DISABLE(ETS_I2C0_INUM);
  } else if (_peripheral == PERIPH_I2C1_MODULE) {
    ESP_INTR_DISABLE(ETS_I2C1_INUM);
  }

  gpio_set_direction((gpio_num_t)_uc_pinSCL, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)_uc_pinSCL, GPIO_FLOATING);

  gpio_set_direction((gpio_num_t)_uc_pinSDA, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)_uc_pinSDA, GPIO_FLOATING);

  periph_module_disable(_peripheral);
}

uint8_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool stopBit)
{
  rxBuffer.clear();

  if(quantity == 0)
  {
    return 0;
  }

  size_t bytesRead = 0;
  int result;

  // reset FIFO
  _dev->fifo_conf.tx_fifo_rst = 1;
  _dev->fifo_conf.tx_fifo_rst = 0;
  _dev->fifo_conf.rx_fifo_rst = 1;
  _dev->fifo_conf.rx_fifo_rst = 0;

  result = startTransmission(((address << 1) & 0xFF) | 0x01);

  while (result == 0 && rxBuffer.available() < (int)quantity) {
    uint8_t nack = (rxBuffer.available() == (int)(quantity - 1));
    uint8_t value;

    result = receiveData(value, nack);

    if (result == 0) {
      rxBuffer.store_char(value);
    }
  }

  if (stopBit) {
    stopTransmission();
  }

  if (result != 0) {
    bytesRead = 0;
  } else {
    bytesRead = rxBuffer.available();
  }

  return bytesRead;
}

uint8_t TwoWire::requestFrom(uint8_t address, size_t quantity)
{
  return requestFrom(address, quantity, true);
}

void TwoWire::beginTransmission(uint8_t address) {
  // save address of target and clear buffer
  txAddress = address;
  txBuffer.clear();

  transmissionBegun = true;
}

// Errors:
//  0 : Success
//  1 : Data too long
//  2 : NACK on transmit of address
//  3 : NACK on transmit of data
//  4 : Other error
uint8_t TwoWire::endTransmission(bool stopBit)
{
  int result;

  // reset FIFO
  _dev->fifo_conf.tx_fifo_rst = 1;
  _dev->fifo_conf.tx_fifo_rst = 0;

  transmissionBegun = false ;

  result = startTransmission((txAddress << 1) & 0xFF);

  while (result == 0 && txBuffer.available()) {
    result = sendData(txBuffer.read_char());
  }

  if (stopBit) {
    stopTransmission();
  }

  return result;
}

uint8_t TwoWire::endTransmission()
{
  return endTransmission(true);
}

size_t TwoWire::write(uint8_t ucData)
{
  // No writing, without begun transmission or a full buffer
  if ( !transmissionBegun || txBuffer.isFull() )
  {
    return 0 ;
  }

  txBuffer.store_char( ucData ) ;

  return 1 ;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
  //Try to store all data
  for(size_t i = 0; i < quantity; ++i)
  {
    //Return the number of data stored, when the buffer is full (if write return 0)
    if(!write(data[i]))
      return i;
  }

  //All data stored
  return quantity;
}

int TwoWire::available(void)
{
  return rxBuffer.available();
}

int TwoWire::read(void)
{
  return rxBuffer.read_char();
}

int TwoWire::peek(void)
{
  return rxBuffer.peek();
}

void TwoWire::flush(void)
{
  // Do nothing, use endTransmission(..) to force
  // data transfer.
}

void TwoWire::onReceive(void(*function)(int))
{
  onReceiveCallback = function;
}

void TwoWire::onService(void)
{
  if (_dev->int_status.rx_fifo_full) {
    while(!rxBuffer.isFull() && _dev->status_reg.rx_fifo_cnt) {
      rxBuffer.store_char(_dev->fifo_data.data);
    }

    // reset fifo
    _dev->fifo_conf.rx_fifo_rst = 1;
    _dev->fifo_conf.rx_fifo_rst = 0;

    _dev->int_clr.rx_fifo_full = 1;
  }

  if (_dev->int_status.slave_tran_comp) {
    if (_dev->status_reg.slave_rw == 0) {
      //write
      if (rxBuffer.available() > 0) {
        // repeated start detected
        if (onReceiveCallback) {
          onReceiveCallback(rxBuffer.available());
        }

        rxBuffer.clear();
      }

      while(!rxBuffer.isFull() && (_dev->status_reg.rx_fifo_cnt > 1)) {
        rxBuffer.store_char(_dev->fifo_data.data);
      }
    }

    _dev->int_clr.slave_tran_comp = 1;
  }

  if (_dev->int_status.trans_complete) {
    while(!rxBuffer.isFull() && _dev->status_reg.rx_fifo_cnt) {
      rxBuffer.store_char(_dev->fifo_data.data);
    }

    if (onReceiveCallback && rxBuffer.available()) {
      onReceiveCallback(rxBuffer.available());
    }

    rxBuffer.clear();

    _dev->int_clr.trans_complete = 1;
  }
}

uint8_t TwoWire::startTransmission(uint8_t address)
{
  // start
  _dev->command[0].byte_num = 0;
  _dev->command[0].ack_en = 0;
  _dev->command[0].ack_exp = 0;
  _dev->command[0].ack_val = 0;
  _dev->command[0].op_code = 0; // RSTART
  _dev->command[0].done = 0;

  // address
  _dev->fifo_data.data = address;

  _dev->command[1].byte_num = 1;
  _dev->command[1].ack_en = 1;
  _dev->command[1].ack_exp = 0;
  _dev->command[1].ack_val = 0;
  _dev->command[1].op_code = 1; // WRITE
  _dev->command[1].done = 0;

  // end
  _dev->command[2].byte_num = 0;
  _dev->command[2].ack_en = 0;
  _dev->command[2].ack_exp = 0;
  _dev->command[2].ack_val = 0;
  _dev->command[2].op_code = 4; // END
  _dev->command[2].done = 0;

  // send commands and wait
  _dev->int_clr.val = 0xFFFFFFFF;
  _dev->ctr.trans_start = 1;
  while(!_dev->command[1].done && !_dev->int_raw.arbitration_lost && !_dev->int_raw.time_out && !_dev->int_raw.ack_err);

  if (_dev->int_raw.arbitration_lost) {
    return 4;
  } else if (_dev->int_raw.ack_err) {
    return 2;
  } else if (_dev->int_raw.time_out) {
    return 2;
  } else {
    return 0;
  }
}

uint8_t TwoWire::stopTransmission()
{
  // stop
  _dev->command[0].byte_num = 0;
  _dev->command[0].ack_en = 0;
  _dev->command[0].ack_exp = 0;
  _dev->command[0].ack_val = 0;
  _dev->command[0].op_code = 3; // STOP
  _dev->command[0].done = 0;

  // end
  _dev->command[1].byte_num = 0;
  _dev->command[1].ack_en = 0;
  _dev->command[1].ack_exp = 0;
  _dev->command[1].ack_val = 0;
  _dev->command[1].op_code = 4; // END
  _dev->command[1].done = 0;

  // send commands and wait
  _dev->int_clr.val = 0xFFFFFFFF;
  _dev->ctr.trans_start = 1;
  while(!_dev->command[0].done && !_dev->int_raw.arbitration_lost && !_dev->int_raw.time_out && !_dev->int_raw.ack_err);

  return 0;
}

uint8_t TwoWire::sendData(uint8_t b)
{
  // address
  _dev->fifo_data.data = b;

  _dev->command[0].byte_num = 1;
  _dev->command[0].ack_en = 1;
  _dev->command[0].ack_exp = 0;
  _dev->command[0].ack_val = 0;
  _dev->command[0].op_code = 1; // WRITE
  _dev->command[0].done = 0;

  // end
  _dev->command[1].byte_num = 0;
  _dev->command[1].ack_en = 0;
  _dev->command[1].ack_exp = 0;
  _dev->command[1].ack_val = 0;
  _dev->command[1].op_code = 4; // END
  _dev->command[1].done = 0;

  // send commands and wait
  _dev->int_clr.val = 0xFFFFFFFF;
  _dev->ctr.trans_start = 1;
  while(!_dev->command[0].done && !_dev->int_raw.arbitration_lost && !_dev->int_raw.time_out && !_dev->int_raw.ack_err);

  if (_dev->int_raw.arbitration_lost) {
    return 4;
  } else if (_dev->int_raw.ack_err) {
    return 3;
  } else if (_dev->int_raw.time_out) {
    return 3;
  } else {
    return 0;
  }
}

uint8_t TwoWire::receiveData(uint8_t& value, uint8_t nack)
{
  _dev->command[0].byte_num = 1;
  _dev->command[0].ack_en = 0;
  _dev->command[0].ack_exp = 0;
  _dev->command[0].ack_val = nack;
  _dev->command[0].op_code = 2; // READ
  _dev->command[0].done = 0;

  // end
  _dev->command[1].byte_num = 0;
  _dev->command[1].ack_en = 0;
  _dev->command[1].ack_exp = 0;
  _dev->command[1].ack_val = 0;
  _dev->command[1].op_code = 4; // END
  _dev->command[1].done = 0;

  // send commands and wait
  _dev->int_clr.val = 0xFFFFFFFF;
  _dev->ctr.trans_start = 1;
  while(!_dev->command[0].done && !_dev->int_raw.arbitration_lost && !_dev->int_raw.time_out && !_dev->int_raw.ack_err);

  if (_dev->int_raw.arbitration_lost) {
    return 4;
  } else if (_dev->int_raw.ack_err) {
    return 3;
  } else if (_dev->int_raw.time_out) {
    return 3;
  } else {
    value = _dev->fifo_data.data;
    return 0;
  }
}

void TwoWire::onService(void* arg)
{
  TwoWire* wire = (TwoWire*)arg;

  wire->onService();
}

TwoWire Wire(PERIPH_I2C0_MODULE, &I2C0, SDA, SCL);
