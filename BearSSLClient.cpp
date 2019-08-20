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

#include <ArduinoECCX08.h>

#include "ArduinoBearSSL.h"
#include "BearSSLTrustAnchors.h"
#include "utility/eccX08_asn1.h"

#include "BearSSLClient.h"

BearSSLClient::BearSSLClient(Client& client) :
  BearSSLClient(client, TAs, TAs_NUM)
{
}

BearSSLClient::BearSSLClient(Client& client, const br_x509_trust_anchor* myTAs, int myNumTAs) :
  _client(&client),
  _TAs(myTAs),
  _numTAs(myNumTAs)
{
  _ecKey.curve = 0;
  _ecKey.x = NULL;
  _ecKey.xlen = 0;

  _ecCert.data = NULL;
  _ecCert.data_len = 0;
  _ecCertDynamic = false;
}

BearSSLClient::~BearSSLClient()
{
  if (_ecCertDynamic && _ecCert.data) {
    free(_ecCert.data);
    _ecCert.data = NULL;
  }
}

int BearSSLClient::connect(IPAddress ip, uint16_t port)
{
  if (!_client->connect(ip, port)) {
    return 0;
  }

  return connectSSL(NULL);
}

int BearSSLClient::connect(const char* host, uint16_t port)
{
  if (!_client->connect(host, port)) {
    return 0;
  }

  return connectSSL(host);
}

size_t BearSSLClient::write(uint8_t b)
{
  return write(&b, sizeof(b));
}

size_t BearSSLClient::write(const uint8_t *buf, size_t size)
{
  size_t written = 0;

  while (written < size) {
    int result = br_sslio_write(&_ioc, buf, size);

    if (result < 0) {
      break;
    }

    buf += result;
    written += result;
  }

  if (written == size && br_sslio_flush(&_ioc) < 0) {
    return 0;
  }

  return written;
}

int BearSSLClient::available()
{
  int available = br_sslio_read_available(&_ioc);

  if (available < 0) {
    available = 0;
  }

  return available;
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
  return br_sslio_read(&_ioc, buf, size);
}

int BearSSLClient::peek()
{
  byte b;

  if (br_sslio_peek(&_ioc, &b, sizeof(b)) == sizeof(b)) {
    return b;
  }

  return -1;
}

void BearSSLClient::flush()
{
  br_sslio_flush(&_ioc);

  _client->flush();
}

void BearSSLClient::stop()
{
  if (_client->connected()) {
    if ((br_ssl_engine_current_state(&_sc.eng) & BR_SSL_CLOSED) == 0) {
      br_sslio_close(&_ioc);
    }

    _client->stop();
  }
}

uint8_t BearSSLClient::connected()
{
  if (!_client->connected()) {
    return 0;
  }

  unsigned state = br_ssl_engine_current_state(&_sc.eng);

  if (state == BR_SSL_CLOSED) {
    return 0;
  }

  return 1;
}

BearSSLClient::operator bool()
{
  return (*_client);  
}

void BearSSLClient::setEccSlot(int ecc508KeySlot, const byte cert[], int certLength)
{
  // HACK: put the key slot info. in the br_ec_private_key structure
  _ecKey.curve = 23;
  _ecKey.x = (unsigned char*)ecc508KeySlot;
  _ecKey.xlen = 32;

  _ecCert.data = (unsigned char*)cert;
  _ecCert.data_len = certLength;
  _ecCertDynamic = false;
}

void BearSSLClient::setEccSlot(int ecc508KeySlot, const char cert[])
{
  // try to decode the cert
  br_pem_decoder_context pemDecoder;

  size_t certLen = strlen(cert);

  // free old data
  if (_ecCertDynamic && _ecCert.data) {
    free(_ecCert.data);
    _ecCert.data = NULL;
  }

  // assume the decoded cert is 3/4 the length of the input
  _ecCert.data = (unsigned char*)malloc(((certLen * 3) + 3) / 4);
  _ecCert.data_len = 0;

  br_pem_decoder_init(&pemDecoder);

  while (certLen) {
    size_t len = br_pem_decoder_push(&pemDecoder, cert, certLen);

    cert += len;
    certLen -= len;

    switch (br_pem_decoder_event(&pemDecoder)) {
      case BR_PEM_BEGIN_OBJ:
        br_pem_decoder_setdest(&pemDecoder, BearSSLClient::clientAppendCert, this);
        break;

      case BR_PEM_END_OBJ:
        if (_ecCert.data_len) {
          // done
          setEccSlot(ecc508KeySlot, _ecCert.data, _ecCert.data_len);
          _ecCertDynamic = true;
          return;
        }
        break;

      case BR_PEM_ERROR:
        // failure
        free(_ecCert.data);
        setEccSlot(ecc508KeySlot, NULL, 0);
        return;
    }
  }
}

int BearSSLClient::errorCode()
{
  return br_ssl_engine_last_error(&_sc.eng);
}

int BearSSLClient::connectSSL(const char* host)
{
  // initialize client context with all algorithms and hardcoded trust anchors
  br_ssl_client_init_full(&_sc, &_xc, _TAs, _numTAs);

  // set the buffer in split mode
  br_ssl_engine_set_buffer(&_sc.eng, _iobuf, sizeof(_iobuf), 1);

  // inject entropy in engine
  unsigned char entropy[32];

  if (ECCX08.begin() && ECCX08.locked() && ECCX08.random(entropy, sizeof(entropy))) {
    // ECC508 random success, add custom ECDSA vfry and EC sign
    br_ssl_engine_set_ecdsa(&_sc.eng, eccX08_vrfy_asn1);
    br_x509_minimal_set_ecdsa(&_xc, br_ssl_engine_get_ec(&_sc.eng), br_ssl_engine_get_ecdsa(&_sc.eng));
    
    // enable client auth using the ECCX08
    if (_ecCert.data_len && _ecKey.xlen) {
      br_ssl_client_set_single_ec(&_sc, &_ecCert, 1, &_ecKey, BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN, BR_KEYTYPE_EC, br_ec_get_default(), eccX08_sign_asn1);
    }
  } else {
    // no ECCX08 or random failed, fallback to pseudo random
    for (size_t i = 0; i < sizeof(entropy); i++) {
      entropy[i] = random(0, 255);
    }
  }
  br_ssl_engine_inject_entropy(&_sc.eng, entropy, sizeof(entropy));

  // set the hostname used for SNI
  br_ssl_client_reset(&_sc, host, 0);

  // get the current time and set it for X.509 validation
  uint32_t now = ArduinoBearSSL.getTime();
  uint32_t days = now / 86400 + 719528;
  uint32_t sec = now % 86400;

  br_x509_minimal_set_time(&_xc, days, sec);

  // use our own socket I/O operations
  br_sslio_init(&_ioc, &_sc.eng, BearSSLClient::clientRead, _client, BearSSLClient::clientWrite, _client);

  br_sslio_flush(&_ioc);

  while (1) {
    unsigned state = br_ssl_engine_current_state(&_sc.eng);

    if (state & BR_SSL_SENDAPP) {
      break;
    } else if (state & BR_SSL_CLOSED) {
      return 0;
    }
  }

  return 1;
}

// #define DEBUGSERIAL Serial

int BearSSLClient::clientRead(void *ctx, unsigned char *buf, size_t len)
{
  Client* c = (Client*)ctx;

  if (!c->connected()) {
    return -1;
  }

  int result = c->read(buf, len);
  if (result == -1) {
    return 0;
  }

#ifdef DEBUGSERIAL
  DEBUGSERIAL.print("BearSSLClient::clientRead - ");
  DEBUGSERIAL.print(result);
  DEBUGSERIAL.print(" - ");  
  for (size_t i = 0; i < result; i++) {
    byte b = buf[i];

    if (b < 16) {
      DEBUGSERIAL.print("0");
    }
    DEBUGSERIAL.print(b, HEX);
  }
  DEBUGSERIAL.println();
#endif

  return result;
}

int BearSSLClient::clientWrite(void *ctx, const unsigned char *buf, size_t len)
{
  Client* c = (Client*)ctx;

#ifdef DEBUGSERIAL
  DEBUGSERIAL.print("BearSSLClient::clientWrite - ");
  DEBUGSERIAL.print(len);
  DEBUGSERIAL.print(" - ");
  for (size_t i = 0; i < len; i++) {
    byte b = buf[i];

    if (b < 16) {
      DEBUGSERIAL.print("0");
    }
    DEBUGSERIAL.print(b, HEX);
  }
  DEBUGSERIAL.println();
#endif

  if (!c->connected()) {
    return -1;
  }

  int result = c->write(buf, len);
  if (result == 0) {
    return -1;
  }

  return result;
}

void BearSSLClient::clientAppendCert(void *ctx, const void *data, size_t len)
{
  BearSSLClient* c = (BearSSLClient*)ctx;

  memcpy(&c->_ecCert.data[c->_ecCert.data_len], data, len);
  c->_ecCert.data_len += len;
}
