#include "ArduinoBearSsl.h"

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

extern "C" {
  #include <sys/time.h>
  
  int _gettimeofday(struct timeval* tp, void* /*tzvp*/)
  {
    tp->tv_sec = ArduinoBearSSL.getTime();
    tp->tv_usec = 0;

    return 0;
  }
}
