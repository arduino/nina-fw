#ifndef _ARDUINO_BEAR_SSL_H_
#define _ARDUINO_BEAR_SSL_H_

#include "BearSSLClient.h"

class ArduinoBearSSLClass {
public:
  ArduinoBearSSLClass();
  virtual ~ArduinoBearSSLClass();

  unsigned long getTime();
  void onGetTime(unsigned long(*)(void));

private:
  unsigned long (*_onGetTimeCallback)(void);
};

extern ArduinoBearSSLClass ArduinoBearSSL;

#endif
