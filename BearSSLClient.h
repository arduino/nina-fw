#ifndef _BEAR_SSL_CLIENT_
#define _BEAR_SSL_CLIENT_

#include <Arduino.h>
#include <Client.h>

class BearSSLClient : public Client {

public:
  BearSSLClient(Client& client);
  virtual ~BearSSLClient();

  virtual int connect(IPAddress ip, uint16_t port);
  virtual int connect(const char* host, uint16_t port);
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int available();
  virtual int read();
  virtual int read(uint8_t *buf, size_t size);
  virtual int peek();
  virtual void flush();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();

  using Print::write;

private:
  Client* _client;
};

#endif
