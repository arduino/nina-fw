#ifndef _ECCX08_H_
#define _ECCX08_H_

#include <Arduino.h>
#include <Wire.h>

class ECCX08Class
{
public:
  ECCX08Class(TwoWire& wire, uint8_t address);
  virtual ~ECCX08Class();

  int begin();
  void end();

  String serialNumber();

  int random(byte data[], size_t length);

  int generatePrivateKey(int slot, byte publicKey[]);
  int generatePublicKey(int slot, byte publicKey[]);

  int ecdsaVerify(const byte message[], const byte signature[], const byte pubkey[]);
  int ecSign(int slot, const byte message[], byte signature[]);

  int readSlot(int slot, byte data[], int length);
  int writeSlot(int slot, const byte data[], int length);

  int locked();
  int writeConfiguration(const byte data[]);
  int readConfiguration(byte data[]);
  int lock();

private:
  int wakeup();
  int sleep();
  int idle();

  long version();
  int challenge(const byte message[]);
  int verify(const byte signature[], const byte pubkey[]);
  int sign(int slot, byte signature[]);

  int read(int zone, int address, byte buffer[], int length);
  int write(int zone, int address, const byte buffer[], int length);
  int lock(int zone);

  int addressForSlotOffset(int slot, int offset);

  int sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2, const byte data[] = NULL, size_t dataLength = 0);
  int receiveResponse(void* response, size_t length);
  uint16_t crc16(const byte data[], size_t length);

private:
  TwoWire* _wire;
  uint8_t _address;

  static const uint32_t _wakeupFrequency;
  static const uint32_t _normalFrequency;
};

extern ECCX08Class ECCX08;

#endif
