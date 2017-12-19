#ifndef _ECC508_H_
#define _ECC508_H_

#include <Arduino.h>
#include <Wire.h>

class ECC508Class
{
public:
  ECC508Class(TwoWire& wire, uint8_t address);
  virtual ~ECC508Class();

  int begin();
  void end();

  String serialNumber();

  int random(byte data[], size_t length);

  int generatePrivateKey(int slot, byte publicKey[]);
  int generatePublicKey(int slot, byte publicKey[]);

  int ecdsaVerify(const byte message[], const byte signature[], const byte pubkey[]);
  int ecSign(int slot, const byte message[], byte signature[]);

  int locked();
  int writeConfiguration(const byte data[]);
  int readConfiguration(byte data[]);
  int lock();

private:
  int wakeup();
  int sleep();
  int idle();

  int version();
  int challenge(const byte message[]);
  int verify(const byte signature[], const byte pubkey[]);
  int sign(int slot, byte signature[]);

  int read(int zone, int address, byte buffer[], int length);
  int write(int zone, int address, const byte buffer[], int length);
  int lock(int zone);

  int sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2, const byte data[] = NULL, size_t dataLength = 0);
  int receiveResponse(void* response, size_t length);
  uint16_t crc16(const byte data[], size_t length);

private:
  TwoWire* _wire;
  uint8_t _address;
};

extern ECC508Class ECC508;

#endif
