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
    if (!sendCommand(0x1b, 0x00, 0x0000)) {
      return 0;
    }

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

int ECC508Class::ecdsaVerify(const byte message[], const byte signature[], const byte pubkey[])
{
  if (!challenge(message)) {
    return 0;
  }

  return verify(signature, pubkey);
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

  if (!sendCommand(0x30, 0x00, 0x0000)) {
    return 0;
  }

  delay(1);

  if (!receiveResponse(&version, sizeof(version))) {
    return 0;
  }

  delay(1);
  idle();

  return version;
}

int ECC508Class::challenge(const byte message[])
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  // Nounce, pass through
  if (!sendCommand(0x16, 0x03, 0x0000, message, 32)) {
    return 0;
  }

  delay(7);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECC508Class::verify(const byte signature[], const byte pubkey[])
{
  uint8_t status;

  if (!wakeup()) {
    return 0;
  }

  byte data[128];
  memcpy(&data[0], signature, 64);
  memcpy(&data[64], pubkey, 64);

  // Verify, external, P256
  if (!sendCommand(0x45, 0x02, 0x0004, data, sizeof(data))) {
    return 0;
  }

  delay(58);

  if (!receiveResponse(&status, sizeof(status))) {
    return 0;
  }

  delay(1);
  idle();

  if (status != 0) {
    return 0;
  }

  return 1;
}

int ECC508Class::sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2, const byte data[], size_t dataLength)
{
  byte command[8 + dataLength]; // 1 for type, 1 for length, 1 for opcode, 1 for param1, 2 for param2, 2 for crc

  command[0] = 0x03;
  command[1] = sizeof(command) - 1;
  command[2] = opcode;
  command[3] = param1;
  memcpy(&command[4], &param2, sizeof(param2));
  memcpy(&command[6], data, dataLength);

  uint16_t crc = crc16(&command[1], 8 - 3 + dataLength);
  memcpy(&command[6 + dataLength], &crc, sizeof(crc));

  if (_wire->sendTo(_address, command, 8 + dataLength) != 0) {
    return 0;
  }

  return 1;
}

int ECC508Class::receiveResponse(void* response, size_t length)
{
  int retries = 20;
  int responseSize = length + 3; // 1 for length header, 2 for CRC
  byte responseBuffer[responseSize];

  while (_wire->requestFrom(_address, responseBuffer, responseSize) != responseSize && retries--);

  // make sure length matches
  if (responseBuffer[0] != responseSize) {
    return 0;
  }

  // verify CRC
  uint16_t responseCrc = responseBuffer[length + 1] | (responseBuffer[length + 2] << 8);
  if (responseCrc != crc16(responseBuffer, responseSize - 2)) {
    return 0;
  }
  
  memcpy(response, &responseBuffer[1], length);

  return 1;
}

uint16_t ECC508Class::crc16(const byte data[], size_t length)
{
  if (data == NULL || length == 0) {
    return 0;
  }

  uint16_t crc = 0;

  while (length) {
    byte b = *data;

    for (uint8_t shift = 0x01; shift > 0x00; shift <<= 1) {
      uint8_t dataBit = (b & shift) ? 1 : 0;
      uint8_t crcBit = crc >> 15;

      crc <<= 1;
      
      if (dataBit != crcBit) {
        crc ^= 0x8005;
      }
    }

    length--;
    data++;
  }

  return crc;
}

ECC508Class ECC508(Wire, 0x60);
