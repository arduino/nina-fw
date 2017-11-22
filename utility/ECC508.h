#ifndef _ECC508_H_
#define _ECC508_H_

#include <Wire.h>

class ECC508Class
{
public:
  ECC508Class(TwoWire& wire, uint8_t address);
  virtual ~ECC508Class();

  int begin();
  void end();

  int random(byte data[], size_t length);

private:
  int wakeup();
  int sleep();
  int idle();

  int version();

  int sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2);
  int receiveResponse(void* response, size_t length);
  uint16_t crc16(byte data[], size_t length);

private:
  TwoWire* _wire;
  uint8_t _address;
};

extern ECC508Class ECC508;

#endif
