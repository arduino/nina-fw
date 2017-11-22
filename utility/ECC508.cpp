#include <Arduino.h>

#include "ECC508.h"

ECC508Class::ECC508Class(TwoWire& wire, uint8_t address) :
  _wire(&wire),
  _address(address)
{
}

ECC508Class::~ECC508Class()
{
}

int ECC508Class::begin()
{
  _wire->begin();
  _wire->setClock(100000);

  if (version() != 0x500000) {
    return 0;
  }

  return 1;
}

void ECC508Class::end()
{
  _wire->end();
}

int ECC508Class::random(byte data[], size_t length)
{
  if (!wakeup()) {
    return 0;
  }

  while (length) {
    _wire->beginTransmission(_address);
    _wire->write(0x03);
    _wire->write(0x07);
    _wire->write(0x1b);
    _wire->write(0x00);
    _wire->write(0x00);
    _wire->write(0x00);
    _wire->write(0x24);
    _wire->write(0xcd);
    _wire->endTransmission();

    delay(23);

    byte response[32];

    if (!receiveResponse(response, sizeof(response))) {
      return 0;
    }

    int copyLength = min(32, length);
    memcpy(data, response, copyLength);

    length -= copyLength;
    data += copyLength;
  }

  delay(1);

  idle();

  return 1;
}

int ECC508Class::wakeup()
{
  _wire->beginTransmission(0x00);
  _wire->endTransmission();

  delayMicroseconds(800);

  byte response;

  if (!receiveResponse(&response, sizeof(response)) || response != 0x11) {
    return 0;
  }

  return 1;
}

int ECC508Class::sleep()
{
  _wire->beginTransmission(_address);
  _wire->write(0x01);

  if (_wire->endTransmission() != 0) {
    return 0;
  }

  return 1;
}

int ECC508Class::idle()
{
  _wire->beginTransmission(_address);
  _wire->write(0x02);

  if (_wire->endTransmission() != 0) {
    return 0;
  }

  return 1;
}

int ECC508Class::version()
{
  uint32_t version = 0;

  if (!wakeup()) {
    return 0;
  }

  _wire->beginTransmission(_address);
  _wire->write(0x03);
  _wire->write(0x07);
  _wire->write(0x30);
  _wire->write(0x00);
  _wire->write(0x00);
  _wire->write(0x00);
  _wire->write(0x03);
  _wire->write(0x5d);
  _wire->endTransmission();

  delay(2);

  if (!receiveResponse(&version, sizeof(version))) {
    return 0;
  }

  delay(1);
  idle();

  return version;
}

int ECC508Class::receiveResponse(void* response, size_t length)
{
  int retries = 20;
  int responseSize = length + 3; // 1 for length header, 2 for CRC
  byte responseBuffer[responseSize];

  while (_wire->requestFrom(_address, responseBuffer, responseSize) != responseSize && retries--);

  if (responseBuffer[0] != responseSize) {
    return 0;
  }

  // TODO: verify CRC
  
  memcpy(response, &responseBuffer[1], length);

  return 1;
}

ECC508Class ECC508(Wire, 0x60);
