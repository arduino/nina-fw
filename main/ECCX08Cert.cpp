/*
   This file is part of ArduinoIoTCloud.

   Copyright 2019 ARDUINO SA (http://www.arduino.cc/)

   This software is released under the GNU General Public License version 3,
   which covers the main part of arduino-cli.
   The terms of this license can be found at:
   https://www.gnu.org/licenses/gpl-3.0.en.html

   You can be released from the requirements of the above licenses by purchasing
   a commercial license. Buying such a license is mandatory if you want to modify or
   otherwise use the software for commercial activities involving the Arduino
   software without disclosing the source code of your own applications. To purchase
   a commercial license, send an email to license@arduino.cc.
*/

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "bearssl/bearssl_hash.h"

#include <ArduinoECCX08.h>

#include "ECCX08Cert.h"

/******************************************************************************
 * DEFINE
 ******************************************************************************/

#define ASN1_INTEGER           0x02
#define ASN1_BIT_STRING        0x03
#define ASN1_NULL              0x05
#define ASN1_OBJECT_IDENTIFIER 0x06
#define ASN1_PRINTABLE_STRING  0x13
#define ASN1_SEQUENCE          0x30
#define ASN1_SET               0x31

struct __attribute__((__packed__)) CompressedCert {
  byte signature[64];
  byte dates[3];
  byte unused[5];
};

#define SERIAL_NUMBER_LENGTH            16
#define AUTHORITY_KEY_IDENTIFIER_LENGTH 20

struct __attribute__((__packed__)) SerialNumberAndAuthorityKeyIdentifier {
  byte serialNumber[SERIAL_NUMBER_LENGTH];
  byte authorityKeyIdentifier[AUTHORITY_KEY_IDENTIFIER_LENGTH];
};

/******************************************************************************
 * LOCAL MODULE FUNCTIONS
 ******************************************************************************/

static String base64Encode(const byte in[], unsigned int length, const char* prefix, const char* suffix) {
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

/******************************************************************************
 * CTOR/DTOR
 ******************************************************************************/

ECCX08CertClass::ECCX08CertClass() :
  _keySlot(-1),
  _compressedCertSlot(-1),
  _serialNumberAndAuthorityKeyIdentifierSlot(-1),
  _bytes(NULL),
  _length(0) {
}

ECCX08CertClass::~ECCX08CertClass() {
  if (_bytes) {
    free(_bytes);
    _bytes = NULL;
  }
}

/******************************************************************************
 * PUBLIC MEMBER FUNCTIONS
 ******************************************************************************/

int ECCX08CertClass::beginCSR(int keySlot, bool newPrivateKey) {
  if (keySlot < 0 || keySlot > 8) {
    return 0;
  }

  _keySlot = keySlot;

  if (newPrivateKey) {
    if (!ECCX08.generatePrivateKey(_keySlot, _temp)) {
      return 0;
    }
  } else {
    if (!ECCX08.generatePublicKey(_keySlot, _temp)) {
      return 0;
    }
  }

  return 1;
}

String ECCX08CertClass::endCSR() {
  int versionLen = versionLength();
  int subjectLen = issuerOrSubjectLength(_subjectCountryName,
                                         _subjectStateProvinceName,
                                         _subjectLocalityName,
                                         _subjectOrganizationName,
                                         _subjectOrganizationalUnitName,
                                         _subjectCommonName);
  int subjectHeaderLen = sequenceHeaderLength(subjectLen);
  int publicKeyLen = publicKeyLength();

  int csrInfoLen = versionLen + subjectHeaderLen + subjectLen + publicKeyLen + 2;
  int csrInfoHeaderLen = sequenceHeaderLength(csrInfoLen);

  byte csrInfo[csrInfoHeaderLen + csrInfoLen];
  byte* out = csrInfo;

  appendSequenceHeader(csrInfoLen, out);
  out += csrInfoHeaderLen;

  // version
  appendVersion(0x00, out);
  out += versionLen;

  // subject
  appendSequenceHeader(subjectLen, out);
  out += subjectHeaderLen;
  appendIssuerOrSubject(_subjectCountryName,
                        _subjectStateProvinceName,
                        _subjectLocalityName,
                        _subjectOrganizationName,
                        _subjectOrganizationalUnitName,
                        _subjectCommonName, out);
  out += subjectLen;

  // public key
  appendPublicKey(_temp, out);
  out += publicKeyLen;

  // terminator
  *out++ = 0xa0;
  *out++ = 0x00;

  br_sha256_context sha256Context;
  byte csrInfoSha256[64];
  byte signature[64];

  br_sha256_init(&sha256Context);
  br_sha256_update(&sha256Context, csrInfo, csrInfoHeaderLen + csrInfoLen);
  br_sha256_out(&sha256Context, csrInfoSha256);

  if (!ECCX08.ecSign(_keySlot, csrInfoSha256, signature)) {
    return "";
  }

  int signatureLen = signatureLength(signature);
  int csrLen = csrInfoHeaderLen + csrInfoLen + signatureLen;
  int csrHeaderLen = sequenceHeaderLength(csrLen);

  byte csr[csrLen + csrHeaderLen];
  out = csr;

  appendSequenceHeader(csrLen, out);
  out += csrHeaderLen;

  // info
  memcpy(out, csrInfo, csrInfoHeaderLen + csrInfoLen);
  out += (csrInfoHeaderLen + csrInfoLen);

  // signature
  appendSignature(signature, out);
  out += signatureLen;

  return base64Encode(csr, csrLen + csrHeaderLen, "-----BEGIN CERTIFICATE REQUEST-----\n", "\n-----END CERTIFICATE REQUEST-----\n");
}

int ECCX08CertClass::beginStorage(int compressedCertSlot, int serialNumberAndAuthorityKeyIdentifierSlot) {
  if (compressedCertSlot < 8 || compressedCertSlot > 15) {
    return 0;
  }

  if (serialNumberAndAuthorityKeyIdentifierSlot < 8 || serialNumberAndAuthorityKeyIdentifierSlot > 15) {
    return 0;
  }

  _compressedCertSlot = compressedCertSlot;
  _serialNumberAndAuthorityKeyIdentifierSlot = serialNumberAndAuthorityKeyIdentifierSlot;

  memset(_temp, 0x00, sizeof(_temp));

  return 1;
}

void ECCX08CertClass::setSignature(byte signature[]) {
  struct CompressedCert* compressedCert = (struct CompressedCert*)_temp;

  memcpy(compressedCert->signature, signature, 64);
}

void ECCX08CertClass::setIssueYear(int issueYear) {
  struct CompressedCert* compressedCert = (struct CompressedCert*)_temp;

  compressedCert->dates[0] &= 0x07;
  compressedCert->dates[0] |= (issueYear - 2000) << 3;
}

void ECCX08CertClass::setIssueMonth(int issueMonth) {
  struct CompressedCert* compressedCert = (struct CompressedCert*)_temp;

  compressedCert->dates[0] &= 0xf8;
  compressedCert->dates[0] |= issueMonth >> 1;

  compressedCert->dates[1] &= 0x7f;
  compressedCert->dates[1] |= issueMonth << 7;
}

void ECCX08CertClass::setIssueDay(int issueDay) {
  struct CompressedCert* compressedCert = (struct CompressedCert*)_temp;

  compressedCert->dates[1] &= 0x83;
  compressedCert->dates[1] |= issueDay << 2;
}

void ECCX08CertClass::setIssueHour(int issueHour) {
  struct CompressedCert* compressedCert = (struct CompressedCert*)_temp;

  compressedCert->dates[2] &= 0x1f;
  compressedCert->dates[2] |= issueHour << 5;

  compressedCert->dates[1] &= 0xfc;
  compressedCert->dates[1] |= issueHour >> 3;
}

void ECCX08CertClass::setExpireYears(int expireYears) {
  struct CompressedCert* compressedCert = (struct CompressedCert*)_temp;

  compressedCert->dates[2] &= 0xe0;
  compressedCert->dates[2] |= expireYears;
}

void ECCX08CertClass::setSerialNumber(const byte serialNumber[]) {
  memcpy(&_temp[72], serialNumber, SERIAL_NUMBER_LENGTH);
}

void ECCX08CertClass::setAuthorityKeyIdentifier(const byte authorityKeyIdentifier[]) {
  memcpy(&_temp[88], authorityKeyIdentifier, AUTHORITY_KEY_IDENTIFIER_LENGTH);
}

int ECCX08CertClass::endStorage() {
  if (!ECCX08.writeSlot(_compressedCertSlot, &_temp[0], 72)) {
    return 0;
  }

  if (!ECCX08.writeSlot(_serialNumberAndAuthorityKeyIdentifierSlot, &_temp[72], SERIAL_NUMBER_LENGTH + AUTHORITY_KEY_IDENTIFIER_LENGTH)) {
    return 0;
  }

  return 1;
}

int ECCX08CertClass::beginReconstruction(int keySlot, int compressedCertSlot, int serialNumberAndAuthorityKeyIdentifierSlot) {
  if (keySlot < 0 || keySlot > 8) {
    return 0;
  }

  if (compressedCertSlot < 8 || compressedCertSlot > 15) {
    return 0;
  }

  if (serialNumberAndAuthorityKeyIdentifierSlot < 8 || serialNumberAndAuthorityKeyIdentifierSlot > 15) {
    return 0;
  }

  _keySlot = keySlot;
  _compressedCertSlot = compressedCertSlot;
  _serialNumberAndAuthorityKeyIdentifierSlot = serialNumberAndAuthorityKeyIdentifierSlot;

  return 1;
}

int ECCX08CertClass::endReconstruction() {
  byte publicKey[64];
  struct CompressedCert compressedCert;
  struct SerialNumberAndAuthorityKeyIdentifier serialNumberAndAuthorityKeyIdentifier;

  if (!ECCX08.generatePublicKey(_keySlot, publicKey)) {
    return 0;
  }

  if (!ECCX08.readSlot(_compressedCertSlot, (byte*)&compressedCert, sizeof(compressedCert))) {
    return 0;
  }

  if (!ECCX08.readSlot(_serialNumberAndAuthorityKeyIdentifierSlot, (byte*)&serialNumberAndAuthorityKeyIdentifier, sizeof(serialNumberAndAuthorityKeyIdentifier))) {
    return 0;
  }

  // dates
  int year = (compressedCert.dates[0] >> 3) + 2000;
  int month = ((compressedCert.dates[0] & 0x07) << 1) | (compressedCert.dates[1] >> 7);
  int day = (compressedCert.dates[1] & 0x7c) >> 2;
  int hour = ((compressedCert.dates[1] & 0x03) << 3) | (compressedCert.dates[2] >> 5);
  int expireYears = (compressedCert.dates[2] & 0x1f);

  int datesSize = 30;

  if (year > 2049) {
    // two more bytes for GeneralizedTime
    datesSize += 2;
  }

  if ((year + expireYears) > 2049) {
    // two more bytes for GeneralizedTime
    datesSize += 2;
  }

  int serialNumberLen = serialNumberLength(serialNumberAndAuthorityKeyIdentifier.serialNumber);

  int issuerLen = issuerOrSubjectLength(_issuerCountryName,
                                        _issuerStateProvinceName,
                                        _issuerLocalityName,
                                        _issuerOrganizationName,
                                        _issuerOrganizationalUnitName,
                                        _issuerCommonName);

  int issuerHeaderLen = sequenceHeaderLength(issuerLen);

  int subjectLen = issuerOrSubjectLength(_subjectCountryName,
                                         _subjectStateProvinceName,
                                         _subjectLocalityName,
                                         _subjectOrganizationName,
                                         _subjectOrganizationalUnitName,
                                         _subjectCommonName);

  int subjectHeaderLen = sequenceHeaderLength(subjectLen);

  int publicKeyLen = publicKeyLength();

  int authorityKeyIdentifierLen = authorityKeyIdentifierLength(serialNumberAndAuthorityKeyIdentifier.authorityKeyIdentifier);

  int signatureLen = signatureLength(compressedCert.signature);

  int certInfoLen = 5 + serialNumberLen + 12 + issuerHeaderLen + issuerLen + (datesSize + 2) +
                    subjectHeaderLen + subjectLen + publicKeyLen;

  if (authorityKeyIdentifierLen) {
    certInfoLen += authorityKeyIdentifierLen;
  } else {
    certInfoLen += 4;
  }

  int certInfoHeaderLen = sequenceHeaderLength(certInfoLen);

  int certDataLen = certInfoLen + certInfoHeaderLen + signatureLen;
  int certDataHeaderLen = sequenceHeaderLength(certDataLen);

  _length = certDataLen + certDataHeaderLen;
  _bytes = (byte*)realloc(_bytes, _length);

  if (!_bytes) {
    _length = 0;
    return 0;
  }

  byte* out = _bytes;

  appendSequenceHeader(certDataLen, out);
  out += certDataHeaderLen;

  appendSequenceHeader(certInfoLen, out);
  out += certInfoHeaderLen;

  // version
  *out++ = 0xA0;
  *out++ = 0x03;
  *out++ = 0x02;
  *out++ = 0x01;
  *out++ = 0x02;

  // serial number
  appendSerialNumber(serialNumberAndAuthorityKeyIdentifier.serialNumber, out);
  out += serialNumberLen;

  // ecdsaWithSHA256
  out += appendEcdsaWithSHA256(out);

  // issuer
  appendSequenceHeader(issuerLen, out);
  out += issuerHeaderLen;
  appendIssuerOrSubject(_issuerCountryName,
                        _issuerStateProvinceName,
                        _issuerLocalityName,
                        _issuerOrganizationName,
                        _issuerOrganizationalUnitName,
                        _issuerCommonName, out);
  out += issuerLen;

  *out++ = ASN1_SEQUENCE;
  *out++ = datesSize;
  out += appendDate(year, month, day, hour, 0, 0, out);
  out += appendDate(year + expireYears, month, day, hour, 0, 0, out);

  // subject
  appendSequenceHeader(subjectLen, out);
  out += subjectHeaderLen;
  appendIssuerOrSubject(_subjectCountryName,
                        _subjectStateProvinceName,
                        _subjectLocalityName,
                        _subjectOrganizationName,
                        _subjectOrganizationalUnitName,
                        _subjectCommonName, out);
  out += subjectLen;

  // public key
  appendPublicKey(publicKey, out);
  out += publicKeyLen;

  if (authorityKeyIdentifierLen) {
    appendAuthorityKeyIdentifier(serialNumberAndAuthorityKeyIdentifier.authorityKeyIdentifier, out);
    out += authorityKeyIdentifierLen;
  } else {
    // null sequence
    *out++ = 0xA3;
    *out++ = 0x02;
    *out++ = 0x30;
    *out++ = 0x00;
  }

  // signature
  appendSignature(compressedCert.signature, out);
  out += signatureLen;

  return 1;
}

byte* ECCX08CertClass::bytes() {
  return _bytes;
}

int ECCX08CertClass::length() {
  return _length;
}

void ECCX08CertClass::setIssuerCountryName(const String& countryName) {
  _issuerCountryName = countryName;
}

void ECCX08CertClass::setIssuerStateProvinceName(const String& stateProvinceName) {
  _issuerStateProvinceName = stateProvinceName;
}

void ECCX08CertClass::setIssuerLocalityName(const String& localityName) {
  _issuerLocalityName = localityName;
}

void ECCX08CertClass::setIssuerOrganizationName(const String& organizationName) {
  _issuerOrganizationName = organizationName;
}

void ECCX08CertClass::setIssuerOrganizationalUnitName(const String& organizationalUnitName) {
  _issuerOrganizationalUnitName = organizationalUnitName;
}

void ECCX08CertClass::setIssuerCommonName(const String& commonName) {
  _issuerCommonName = commonName;
}

void ECCX08CertClass::setSubjectCountryName(const String& countryName) {
  _subjectCountryName = countryName;
}

void ECCX08CertClass::setSubjectStateProvinceName(const String& stateProvinceName) {
  _subjectStateProvinceName = stateProvinceName;
}

void ECCX08CertClass::setSubjectLocalityName(const String& localityName) {
  _subjectLocalityName = localityName;
}

void ECCX08CertClass::setSubjectOrganizationName(const String& organizationName) {
  _subjectOrganizationName = organizationName;
}

void ECCX08CertClass::setSubjectOrganizationalUnitName(const String& organizationalUnitName) {
  _subjectOrganizationName = organizationalUnitName;
}

void ECCX08CertClass::setSubjectCommonName(const String& commonName) {
  _subjectCommonName = commonName;
}

int ECCX08CertClass::versionLength() {
  return 3;
}

int ECCX08CertClass::issuerOrSubjectLength(const String& countryName,
    const String& stateProvinceName,
    const String& localityName,
    const String& organizationName,
    const String& organizationalUnitName,
    const String& commonName) {
  int length                       = 0;
  int countryNameLength            = countryName.length();
  int stateProvinceNameLength      = stateProvinceName.length();
  int localityNameLength           = localityName.length();
  int organizationNameLength       = organizationName.length();
  int organizationalUnitNameLength = organizationalUnitName.length();
  int commonNameLength             = commonName.length();

  if (countryNameLength) {
    length += (11 + countryNameLength);
  }

  if (stateProvinceNameLength) {
    length += (11 + stateProvinceNameLength);
  }

  if (localityNameLength) {
    length += (11 + localityNameLength);
  }

  if (organizationNameLength) {
    length += (11 + organizationNameLength);
  }

  if (organizationalUnitNameLength) {
    length += (11 + organizationalUnitNameLength);
  }

  if (commonNameLength) {
    length += (11 + commonNameLength);
  }

  return length;
}

int ECCX08CertClass::publicKeyLength() {
  return (2 + 2 + 9 + 10 + 4 + 64);
}

int ECCX08CertClass::authorityKeyIdentifierLength(const byte authorityKeyIdentifier[]) {
  bool set = false;

  // check if the authority key identifier is non-zero
  for (int i = 0; i < AUTHORITY_KEY_IDENTIFIER_LENGTH; i++) {
    if (authorityKeyIdentifier[i] != 0) {
      set = true;
      break;
    }
  }

  return (set ? 37 : 0);
}

int ECCX08CertClass::signatureLength(const byte signature[]) {
  const byte* r = &signature[0];
  const byte* s = &signature[32];

  int rLength = 32;
  int sLength = 32;

  while (*r == 0x00 && rLength) {
    r++;
    rLength--;
  }

  if (*r & 0x80) {
    rLength++;
  }

  while (*s == 0x00 && sLength) {
    s++;
    sLength--;
  }

  if (*s & 0x80) {
    sLength++;
  }

  return (21 + rLength + sLength);
}

int ECCX08CertClass::serialNumberLength(const byte serialNumber[]) {
  int length = SERIAL_NUMBER_LENGTH;

  while (*serialNumber == 0 && length) {
    serialNumber++;
    length--;
  }

  if (*serialNumber & 0x80) {
    length++;
  }

  return (2 + length);
}

int ECCX08CertClass::sequenceHeaderLength(int length) {
  if (length > 255) {
    return 4;
  } else if (length > 127) {
    return 3;
  } else {
    return 2;
  }
}

void ECCX08CertClass::appendVersion(int version, byte out[]) {
  out[0] = ASN1_INTEGER;
  out[1] = 0x01;
  out[2] = version;
}

void ECCX08CertClass::appendIssuerOrSubject(const String& countryName,
    const String& stateProvinceName,
    const String& localityName,
    const String& organizationName,
    const String& organizationalUnitName,
    const String& commonName,
    byte out[]) {
  if (countryName.length() > 0) {
    out += appendName(countryName, 0x06, out);
  }

  if (stateProvinceName.length() > 0) {
    out += appendName(stateProvinceName, 0x08, out);
  }

  if (localityName.length() > 0) {
    out += appendName(localityName, 0x07, out);
  }

  if (organizationName.length() > 0) {
    out += appendName(organizationName, 0x0a, out);
  }

  if (organizationalUnitName.length() > 0) {
    out += appendName(organizationalUnitName, 0x0b, out);
  }

  if (commonName.length() > 0) {
    out += appendName(commonName, 0x03, out);
  }
}

void ECCX08CertClass::appendPublicKey(const byte publicKey[], byte out[]) {
  int subjectPublicKeyDataLength = 2 + 9 + 10 + 4 + 64;

  // subject public key
  *out++ = ASN1_SEQUENCE;
  *out++ = (subjectPublicKeyDataLength) & 0xff;

  *out++ = ASN1_SEQUENCE;
  *out++ = 0x13;

  // EC public key
  *out++ = ASN1_OBJECT_IDENTIFIER;
  *out++ = 0x07;
  *out++ = 0x2a;
  *out++ = 0x86;
  *out++ = 0x48;
  *out++ = 0xce;
  *out++ = 0x3d;
  *out++ = 0x02;
  *out++ = 0x01;

  // PRIME 256 v1
  *out++ = ASN1_OBJECT_IDENTIFIER;
  *out++ = 0x08;
  *out++ = 0x2a;
  *out++ = 0x86;
  *out++ = 0x48;
  *out++ = 0xce;
  *out++ = 0x3d;
  *out++ = 0x03;
  *out++ = 0x01;
  *out++ = 0x07;

  *out++ = 0x03;
  *out++ = 0x42;
  *out++ = 0x00;
  *out++ = 0x04;

  memcpy(out, publicKey, 64);
}

void ECCX08CertClass::appendAuthorityKeyIdentifier(const byte authorityKeyIdentifier[], byte out[]) {
  // [3]
  *out++ = 0xa3;
  *out++ = 0x23;

  // sequence
  *out++ = ASN1_SEQUENCE;
  *out++ = 0x21;

  // sequence
  *out++ = ASN1_SEQUENCE;
  *out++ = 0x1f;

  // 2.5.29.35 authorityKeyIdentifier(X.509 extension)
  *out++ = 0x06;
  *out++ = 0x03;
  *out++ = 0x55;
  *out++ = 0x1d;
  *out++ = 0x23;

  // octet string
  *out++ = 0x04;
  *out++ = 0x18;

  // sequence
  *out++ = ASN1_SEQUENCE;
  *out++ = 0x16;

  *out++ = 0x80;
  *out++ = 0x14;

  memcpy(out, authorityKeyIdentifier, 20);
}

void ECCX08CertClass::appendSignature(const byte signature[], byte out[]) {
  // signature algorithm
  *out++ = ASN1_SEQUENCE;
  *out++ = 0x0a;
  *out++ = ASN1_OBJECT_IDENTIFIER;
  *out++ = 0x08;

  // ECDSA with SHA256
  *out++ = 0x2a;
  *out++ = 0x86;
  *out++ = 0x48;
  *out++ = 0xce;
  *out++ = 0x3d;
  *out++ = 0x04;
  *out++ = 0x03;
  *out++ = 0x02;

  const byte* r = &signature[0];
  const byte* s = &signature[32];

  int rLength = 32;
  int sLength = 32;

  while (*r == 0 && rLength) {
    r++;
    rLength--;
  }

  while (*s == 0 && sLength) {
    s++;
    sLength--;
  }

  if (*r & 0x80) {
    rLength++;
  }

  if (*s & 0x80) {
    sLength++;
  }

  *out++ = ASN1_BIT_STRING;
  *out++ = (rLength + sLength + 7);
  *out++ = 0;

  *out++ = ASN1_SEQUENCE;
  *out++ = (rLength + sLength + 4);

  *out++ = ASN1_INTEGER;
  *out++ = rLength;
  if ((*r & 0x80) && rLength) {
    *out++ = 0;
    rLength--;
  }
  memcpy(out, r, rLength);
  out += rLength;

  *out++ = ASN1_INTEGER;
  *out++ = sLength;
  if ((*s & 0x80) && sLength) {
    *out++ = 0;
    sLength--;
  }
  memcpy(out, s, sLength);
  out += rLength;
}

void ECCX08CertClass::appendSerialNumber(const byte serialNumber[], byte out[]) {
  int length = SERIAL_NUMBER_LENGTH;

  while (*serialNumber == 0 && length) {
    serialNumber++;
    length--;
  }

  if (*serialNumber & 0x80) {
    length++;
  }

  *out++ = ASN1_INTEGER;
  *out++ = length;

  if (*serialNumber & 0x80) {
    *out++ = 0x00;
    length--;
  }

  memcpy(out, serialNumber, length);
}

int ECCX08CertClass::appendName(const String& name, int type, byte out[]) {
  int nameLength = name.length();

  *out++ = ASN1_SET;
  *out++ = nameLength + 9;

  *out++ = ASN1_SEQUENCE;
  *out++ = nameLength + 7;

  *out++ = ASN1_OBJECT_IDENTIFIER;
  *out++ = 0x03;
  *out++ = 0x55;
  *out++ = 0x04;
  *out++ = type;

  *out++ = ASN1_PRINTABLE_STRING;
  *out++ = nameLength;
  memcpy(out, name.c_str(), nameLength);

  return (nameLength + 11);
}

void ECCX08CertClass::appendSequenceHeader(int length, byte out[]) {
  *out++ = ASN1_SEQUENCE;
  if (length > 255) {
    *out++ = 0x82;
    *out++ = (length >> 8) & 0xff;
  } else if (length > 127) {
    *out++ = 0x81;
  }
  *out++ = (length) & 0xff;
}

int ECCX08CertClass::appendDate(int year, int month, int day, int hour, int minute, int second, byte out[]) {
  bool useGeneralizedTime = (year > 2049);

  if (useGeneralizedTime) {
    *out++ = 0x18;
    *out++ = 0x0f;
    *out++ = '0' + (year / 1000);
    *out++ = '0' + ((year % 1000) / 100);
    *out++ = '0' + ((year % 100) / 10);
    *out++ = '0' + (year % 10);
  } else {
    year -= 2000;

    *out++ = 0x17;
    *out++ = 0x0d;
    *out++ = '0' + (year / 10);
    *out++ = '0' + (year % 10);
  }
  *out++ = '0' + (month / 10);
  *out++ = '0' + (month % 10);
  *out++ = '0' + (day / 10);
  *out++ = '0' + (day % 10);
  *out++ = '0' + (hour / 10);
  *out++ = '0' + (hour % 10);
  *out++ = '0' + (minute / 10);
  *out++ = '0' + (minute % 10);
  *out++ = '0' + (second / 10);
  *out++ = '0' + (second % 10);
  *out++ = 0x5a; // UTC

  return (useGeneralizedTime ? 17 : 15);
}

int ECCX08CertClass::appendEcdsaWithSHA256(byte out[]) {
  *out++ = ASN1_SEQUENCE;
  *out++ = 0x0A;
  *out++ = ASN1_OBJECT_IDENTIFIER;
  *out++ = 0x08;
  *out++ = 0x2A;
  *out++ = 0x86;
  *out++ = 0x48;
  *out++ = 0xCE;
  *out++ = 0x3D;
  *out++ = 0x04;
  *out++ = 0x03;
  *out++ = 0x02;

  return 12;
}
