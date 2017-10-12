#include "BearSSLClient.h"

BearSSLClient::BearSSLClient(Client& client) :
  _client(&client)
{
}

BearSSLClient::~BearSSLClient()
{
}


int BearSSLClient::connect(IPAddress ip, uint16_t port)
{
  return 0;
}

int BearSSLClient::connect(const char* host, uint16_t port)
{
  return 0;
}

size_t BearSSLClient::write(uint8_t b)
{
  return write(&b, sizeof(b));
}

size_t BearSSLClient::write(const uint8_t *buf, size_t size)
{
  return 0;
}

int BearSSLClient::available()
{
  return 0;
}

int BearSSLClient::read()
{
  byte b;

  if (read(&b, sizeof(b)) == sizeof(b)) {
    return b;
  }

  return -1;
}

int BearSSLClient::read(uint8_t *buf, size_t size)
{
  return -1;
}

int BearSSLClient::peek()
{
  return -1;
}

void BearSSLClient::flush()
{

}

void BearSSLClient::stop()
{

}

uint8_t BearSSLClient::connected()
{
  return 0;
}

BearSSLClient::operator bool()
{
  return (*_client);  
}
