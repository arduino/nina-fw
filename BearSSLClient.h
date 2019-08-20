/*
 * Copyright (c) 2018 Arduino SA. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _BEAR_SSL_CLIENT_H_
#define _BEAR_SSL_CLIENT_H_

#ifndef BEAR_SSL_CLIENT_IOBUF_SIZE
#define BEAR_SSL_CLIENT_IOBUF_SIZE 8192 + 85 + 325
#endif

#include <Arduino.h>
#include <Client.h>

#include "bearssl/bearssl.h"

class BearSSLClient : public Client {

public:
  BearSSLClient(Client& client);
  BearSSLClient(Client& client, const br_x509_trust_anchor* myTAs, int myNumTAs);
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

  void setEccSlot(int ecc508KeySlot, const byte cert[], int certLength);
  void setEccSlot(int ecc508KeySlot, const char cert[]);

  int errorCode();

private:
  int connectSSL(const char* host);
  static int clientRead(void *ctx, unsigned char *buf, size_t len);
  static int clientWrite(void *ctx, const unsigned char *buf, size_t len);
  static void clientAppendCert(void *ctx, const void *data, size_t len);

private:
  Client* _client;
  const br_x509_trust_anchor* _TAs;
  int _numTAs;

  br_ec_private_key _ecKey;
  br_x509_certificate _ecCert;
  bool _ecCertDynamic;

  br_ssl_client_context _sc;
  br_x509_minimal_context _xc;
  unsigned char _iobuf[BEAR_SSL_CLIENT_IOBUF_SIZE];
  br_sslio_context _ioc;
};

#endif
