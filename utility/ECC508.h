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

  int receiveResponse(void* response, size_t length);

private:
  TwoWire* _wire;
  uint8_t _address;
};

extern ECC508Class ECC508;

#endif
