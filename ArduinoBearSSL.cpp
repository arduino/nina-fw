#include "ArduinoBearSSL.h"

ArduinoBearSSLClass::ArduinoBearSSLClass() :
  _onGetTimeCallback(NULL)
{
}

ArduinoBearSSLClass::~ArduinoBearSSLClass()
{
}

unsigned long ArduinoBearSSLClass::getTime()
{
  if (_onGetTimeCallback) {
    return _onGetTimeCallback();
  }

  return 0;
}

void ArduinoBearSSLClass::onGetTime(unsigned long(*callback)(void))
{
  _onGetTimeCallback = callback;
}

ArduinoBearSSLClass ArduinoBearSSL;
