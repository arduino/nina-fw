#include "ArduinoBearSSL.h"
#include "BearSSLTrustAnchors.h"
#include "utility/ECCX08.h"
#include "utility/eccX08_asn1.h"


#include "BearSSLClient.h"

BearSSLClient::BearSSLClient(Client& client) :
  _client(&client)
{
  _ecKey.curve = 0;
  _ecKey.x = NULL;
  _ecKey.xlen = 0;

  _ecCert.data = NULL;
  _ecCert.data_len = 0;
}

BearSSLClient::~BearSSLClient()
{
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
  br_sslio_write_all(&_ioc, buf, size);
  br_sslio_flush(&_ioc);

  return size;
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
  /*
   * SSL is a buffered protocol: we make sure that all our request
   * bytes are sent onto the wire.
   */
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

  /*
   * Check whether we closed properly or not. If the engine is
   * closed, then its error status allows to distinguish between
   * a normal closure and a SSL error.
   *
   * If the engine is NOT closed, then this means that the
   * underlying network socket was closed or failed in some way.
   * Note that many Web servers out there do not properly close
   * their SSL connections (they don't send a close_notify alert),
   * which will be reported here as "socket closed without proper
   * SSL termination".
   */
  unsigned state = br_ssl_engine_current_state(&_sc.eng);

  if (state == BR_SSL_CLOSED) {
    // int err = br_ssl_engine_last_error(&_sc.eng);
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
}

int BearSSLClient::connectSSL(const char* host)
{
  /*
   * Initialise the client context:
   * -- Use the "full" profile (all supported algorithms).
   * -- The provided X.509 validation engine is initialised, with
   *    the hardcoded trust anchor.
   */
  br_ssl_client_init_full(&_sc, &_xc, TAs, TAs_NUM);

  /*
   * Set the I/O buffer to the provided array. We allocated a
   * buffer large enough for full-duplex behaviour with all
   * allowed sizes of SSL records, hence we set the last argument
   * to 1 (which means "split the buffer into separate input and
   * output areas").
   */
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

  /*
   * Reset the client context, for a new handshake. We provide the
   * target host name: it will be used for the SNI extension. The
   * last parameter is 0: we are not trying to resume a session.
   */
  br_ssl_client_reset(&_sc, host, 0);

  // get the current time and set it for X.509 validation
  uint32_t now = ArduinoBearSSL.getTime();
  uint32_t days = now / 86400 + 719528;
  uint32_t sec = now % 86400;

  br_x509_minimal_set_time(&_xc, days, sec);

  /*
   * Initialise the simplified I/O wrapper context, to use our
   * SSL client context, and the two callbacks for socket I/O.
   */
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

  int r = c->read(buf, len);

  if (r == -1) {
    return 0;
  }

#ifdef DEBUGSERIAL
  DEBUGSERIAL.print("BearSSLClient::clientRead - ");
  DEBUGSERIAL.print(r);
  DEBUGSERIAL.print(" - ");  
  for (size_t i = 0; i < r; i++) {
    byte b = buf[i];

    if (b < 16) {
      DEBUGSERIAL.print("0");
    }
    DEBUGSERIAL.print(b, HEX);
  }
  DEBUGSERIAL.println();
#endif

  return r;
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

  int w = c->write(buf, len);

  return w;
}
