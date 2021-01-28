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

#ifndef TwoWire_h
#define TwoWire_h

extern "C" {
  #include <driver/periph_ctrl.h>
  #include <soc/i2c_struct.h>
}

#include "Stream.h"

#include "RingBuffer.h"

 // WIRE_HAS_END means Wire has end()
#define WIRE_HAS_END 1

static const uint8_t SDA = 13;
static const uint8_t SCL = 14;

class TwoWire : public Stream
{
  public:
    TwoWire(periph_module_t peripheralModule, i2c_dev_t* i2cDev, uint8_t pinSDA, uint8_t pinSCL);
    void begin();
    void begin(uint8_t);
    void end();
    void setClock(uint32_t);

    void beginTransmission(uint8_t);
    uint8_t endTransmission(bool stopBit);
    uint8_t endTransmission(void);

    uint8_t requestFrom(uint8_t address, size_t quantity, bool stopBit);
    uint8_t requestFrom(uint8_t address, size_t quantity);

    size_t write(uint8_t data);
    size_t write(const uint8_t * data, size_t quantity);

    virtual int available(void);
    virtual int read(void);
    virtual int peek(void);
    virtual void flush(void);
    void onReceive(void(*)(int));

    inline size_t write(unsigned long n) { return write((uint8_t)n); }
    inline size_t write(long n) { return write((uint8_t)n); }
    inline size_t write(unsigned int n) { return write((uint8_t)n); }
    inline size_t write(int n) { return write((uint8_t)n); }
    using Print::write;

  private:
    void onService(void);

    uint8_t startTransmission(uint8_t);
    uint8_t stopTransmission();
    uint8_t sendData(uint8_t);
    uint8_t receiveData(uint8_t& value, uint8_t nack);

    static void onService(void* arg);

  private:
    periph_module_t _peripheral;
    volatile i2c_dev_t* _dev;

    uint8_t _uc_pinSDA;
    uint8_t _uc_pinSCL;

    bool transmissionBegun;

    // RX Buffer
    RingBufferN<256> rxBuffer;

    //TX buffer
    RingBufferN<256> txBuffer;
    uint8_t txAddress;

    // Callback user functions
    void (*onReceiveCallback)(int);

    // TWI clock frequency
    static const uint32_t TWI_CLOCK = 100000;
};

extern TwoWire Wire;

#endif
