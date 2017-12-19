#include "bearssl/bearssl_hash.h"

#include "ECC508.h"

#include "ECC508CSR.h"

static String base64Encode(const byte in[], unsigned int length, const char* prefix, const char* suffix)
{
  static const char* CODES = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

  int b;
  String out;

  int reserveLength = 4 * ((length + 2) / 3) + ((length / 3 * 4) / 76) + strlen(prefix) + strlen(suffix);
  out.reserve(reserveLength);

  if (prefix) {
    out += prefix;
  }
  
  for (unsigned int i = 0; i < length; i += 3) {
    if (i > 0 && (i / 3 * 4) % 76 == 0) { 
      out += '\n'; 
    }

    b = (in[i] & 0xFC) >> 2;
    out += CODES[b];

    b = (in[i] & 0x03) << 4;
    if (i + 1 < length) {
      b |= (in[i + 1] & 0xF0) >> 4;
      out += CODES[b];
      b = (in[i + 1] & 0x0F) << 2;
      if (i + 2 < length) {
         b |= (in[i + 2] & 0xC0) >> 6;
         out += CODES[b];
         b = in[i + 2] & 0x3F;
         out += CODES[b];
      } else {
        out += CODES[b];
        out += '=';
      }
    } else {
      out += CODES[b];
      out += "==";
    }
  }

  if (suffix) {
    out += suffix;
  }

  return out;
}

ECC508CSRClass::ECC508CSRClass()
{
}

ECC508CSRClass::~ECC508CSRClass()
{
}

#define ASN1_INTEGER           0x02
#define ASN1_BIT_STRING        0x03
#define ASN1_NULL              0x05
#define ASN1_OBJECT_IDENTIFIER 0x06
#define ASN1_UTF8_DTRING       0x0C
#define ASN1_SEQUENCE          0x30
#define ASN1_SET               0x31

int ECC508CSRClass::begin(int slot, bool newPrivateKey)
{
  _slot = slot;

  if (newPrivateKey) {
    if (!ECC508.generatePrivateKey(slot, _publicKey)) {
      return 0;
    }
  } else {
    if (!ECC508.generatePublicKey(slot, _publicKey)) {
      return 0;
    }
  }

  return 1;
}

String ECC508CSRClass::end()
{
  byte csrBuffer[1024];
  byte* csr = csrBuffer;
  int csrLength = 0;

  int csrInfoLength = encodeCsrInfo(&csr[4]);

  br_sha256_context sha256Context;
  byte csrInfoSha256[64];
  byte signedCsrInfoSha256[64];

  br_sha256_init(&sha256Context);
  br_sha256_update(&sha256Context, &csr[4], csrInfoLength);
  br_sha256_out(&sha256Context, csrInfoSha256);

  if (!ECC508.ecSign(_slot, csrInfoSha256, signedCsrInfoSha256)) {
    return "";
  }

  int signatureLength = encodeEcdsaSignature(signedCsrInfoSha256, &csr[4 + csrInfoLength]); 

  if ((csrInfoLength + signatureLength) <= 255) {
    csr++;
  }

  csr[csrLength++] = ASN1_SEQUENCE;
  if ((csrInfoLength + signatureLength) > 255) {
    csr[csrLength++] = 0x82;
    csr[csrLength++] = ((csrInfoLength + signatureLength) >> 8) & 0xff;
  } else {
    csr[csrLength++] = 0x81;
  }
  csr[csrLength++] = (csrInfoLength + signatureLength) & 0xff;
  csrLength += (csrInfoLength + signatureLength);

  return base64Encode(csr, csrLength, "-----BEGIN CERTIFICATE REQUEST-----\n", "\n-----END CERTIFICATE REQUEST-----\n");
}

void ECC508CSRClass::setCountryName(const char *countryName)
{
  _countryName = countryName;
}

void ECC508CSRClass::setStateProvinceName(const char* stateProvinceName)
{
  _stateProvinceName = stateProvinceName;
}

void ECC508CSRClass::setLocalityName(const char* localityName)
{
  _localityName = localityName;
}

void ECC508CSRClass::setOrganizationName(const char* organizationName)
{
  _organizationName = organizationName;
}

void ECC508CSRClass::setOrganizationalUnitName(const char* organizationalUnitName)
{
  _organizationalUnitName = organizationalUnitName;
}

void ECC508CSRClass::setCommonName(const char* commonName)
{
  _commonName = commonName;
}

int ECC508CSRClass::encodeCsrInfo(byte buffer[])
{
  int index = 0;

  int countryNameLength = _countryName.length();
  int stateProvinceNameLength = _stateProvinceName.length();
  int localityNameLength = _localityName.length();
  int organizationNameLength = _organizationName.length();
  int organizationalUnitNameLength = _organizationalUnitName.length();
  int commonNameLength = _commonName.length();

  int subjectDataLength = (countryNameLength > 0 ? countryNameLength + 11 : 0) +
                          (stateProvinceNameLength > 0 ? stateProvinceNameLength + 11 : 0) +
                          (localityNameLength > 0 ? localityNameLength + 11 : 0) +
                          (organizationNameLength > 0 ? organizationNameLength + 11 : 0) +
                          (organizationalUnitNameLength > 0 ? organizationalUnitNameLength + 11 : 0) +
                          (commonNameLength > 0 ? commonNameLength + 11 : 0);

  int subjectLength = ((subjectDataLength > 127) ?
                        ((subjectDataLength > 255) ? 3 : 4) :
                        2) + subjectDataLength;

  int subjectPublicKeyDataLength = 5 + sizeof(_publicKey) + 9 + 10;
  int subjectPublicKeyLength = subjectPublicKeyDataLength + 2;

  int attributesLength = 2;

  int csrInfoDataLength = subjectLength + subjectPublicKeyLength + attributesLength;
  int csrInfoLength = ((csrInfoDataLength > 127) ?
                    ((csrInfoDataLength > 255) ? 3 : 4) :
                    2) + csrInfoDataLength;

  buffer[index++] = ASN1_SEQUENCE;
  if (csrInfoLength > 255) {
    buffer[index++] = 0x82;
    buffer[index++] = (csrInfoLength >> 8) & 0xff;
  } else if (csrInfoLength > 127) {
    buffer[index++] = 0x81;
  }
  buffer[index++] = (csrInfoLength) & 0xff;

  // version
  buffer[index++] = ASN1_INTEGER;
  buffer[index++] = 1;
  buffer[index++] = 0;

  // subject
  buffer[index++] = ASN1_SEQUENCE;
  if (subjectDataLength > 255) {
    buffer[index++] = 0x82;
    buffer[index++] = (subjectDataLength >> 8) & 0xff;
  } else if (subjectDataLength > 127) {
    buffer[index++] = 0x81;
  }
  buffer[index++] = (subjectDataLength) & 0xff;

  if (countryNameLength > 0) {
    index += encodeName(_countryName.c_str(), countryNameLength, 0x06, &buffer[index]);
  }

  if (stateProvinceNameLength > 0) {
    index += encodeName(_stateProvinceName.c_str(), stateProvinceNameLength, 0x08, &buffer[index]);
  }

  if (localityNameLength > 0) {
    index += encodeName(_localityName.c_str(), localityNameLength, 0x07, &buffer[index]);
  }

  if (organizationNameLength > 0) {
    index += encodeName(_organizationName.c_str(), organizationNameLength, 0x0a, &buffer[index]);
  }

  if (organizationalUnitNameLength > 0) {
    index += encodeName(_organizationalUnitName.c_str(), organizationalUnitNameLength, 0x0b, &buffer[index]);
  }

  if (commonNameLength > 0) {
    index += encodeName(_commonName.c_str(), commonNameLength, 0x03, &buffer[index]);
  }

  index += encodePublicKey(&buffer[index]);

  buffer[index++] = 0xa0;
  buffer[index++] = 0x00;

  return index;
}

int ECC508CSRClass::encodeEcdsaSignature(const byte signature[], byte buffer[])
{
  int index = 0;

  // signature algorithm
  buffer[index++] = ASN1_SEQUENCE;
  buffer[index++] = 0x0c;
  buffer[index++] = ASN1_OBJECT_IDENTIFIER;
  buffer[index++] = 0x08;

  // ECDSA with SHA256
  buffer[index++] = 0x2a;
  buffer[index++] = 0x86;
  buffer[index++] = 0x48;
  buffer[index++] = 0xce;
  buffer[index++] = 0x3d;
  buffer[index++] = 0x04;
  buffer[index++] = 0x03;
  buffer[index++] = 0x02;

  buffer[index++] = ASN1_NULL;
  buffer[index++] = 0;

  const byte* r = &signature[0];
  const byte* s = &signature[32];

  int rLength = 32;
  int sLength = 32;

  while (r[0] == 0 && rLength) {
    r++;
    rLength--;
  }

  while (s[0] == 0 && sLength) {
    s++;
    sLength--;
  }

  if (r[0] & 0x80) {
    rLength++;  
  }

  if (s[0] & 0x80) {
    sLength++;
  }

  buffer[index++] = ASN1_BIT_STRING;
  buffer[index++] = (rLength + sLength + 7);
  buffer[index++] = 0;

  buffer[index++] = ASN1_SEQUENCE;
  buffer[index++] = (rLength + sLength + 4);

  buffer[index++] = ASN1_INTEGER;
  buffer[index++] = rLength;
  if ((r[0] & 0x80) && rLength) {
    buffer[index++] = 0;
    rLength--;
  }
  memcpy(&buffer[index], r, rLength);
  index += rLength;

  buffer[index++] = ASN1_INTEGER;
  buffer[index++] = sLength;
  if ((s[0] & 0x80) && sLength) {
    buffer[index++] = 0;
    sLength--;
  }
  memcpy(&buffer[index], s, sLength);
  index += sLength;

  return index;
}

int ECC508CSRClass::encodeName(const char* name, int length, int type, byte buffer[])
{
  int index = 0;

  buffer[index++] = ASN1_SET;
  buffer[index++] = length + 9;

  buffer[index++] = ASN1_SEQUENCE;
  buffer[index++] = length + 7;

  buffer[index++] = ASN1_OBJECT_IDENTIFIER;
  buffer[index++] = 0x03;
  buffer[index++] = 0x55;
  buffer[index++] = 0x04;
  buffer[index++] = type;

  buffer[index++] = ASN1_UTF8_DTRING;
  buffer[index++] = length;
  memcpy(&buffer[index], name, length);
  index += length;

  return index;
}

int ECC508CSRClass::encodePublicKey(byte buffer[])
{
  int subjectPublicKeyDataLength = 5 + sizeof(_publicKey) + 9 + 10;
  int index = 0;

  // subject public key
  buffer[index++] = ASN1_SEQUENCE;
  buffer[index++] = (subjectPublicKeyDataLength + 1) & 0xff;

  buffer[index++] = ASN1_SEQUENCE;
  buffer[index++] = 0x13;

  // EC public key
  buffer[index++] = ASN1_OBJECT_IDENTIFIER;
  buffer[index++] = 0x07;
  buffer[index++] = 0x2a;
  buffer[index++] = 0x86;
  buffer[index++] = 0x48;
  buffer[index++] = 0xce;
  buffer[index++] = 0x3d;
  buffer[index++] = 0x02;
  buffer[index++] = 0x01;

  // PRIME 256 v1
  buffer[index++] = ASN1_OBJECT_IDENTIFIER;
  buffer[index++] = 0x08;
  buffer[index++] = 0x2a;
  buffer[index++] = 0x86;
  buffer[index++] = 0x48;
  buffer[index++] = 0xce;
  buffer[index++] = 0x3d;
  buffer[index++] = 0x03;
  buffer[index++] = 0x01;
  buffer[index++] = 0x07;

  buffer[index++] = 0x03;
  buffer[index++] = 0x42;
  buffer[index++] = 0x00;
  buffer[index++] = 0x04;

  memcpy(&buffer[index], _publicKey, sizeof(_publicKey));
  index += sizeof(_publicKey);

  return index;
}

ECC508CSRClass ECC508CSR;
