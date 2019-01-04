/*
  This file is part of the ArduinoECCX08 library.
  Copyright (c) 2019 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "ArduinoECCX08.h"

#include "ECCX08CSR.h"

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

ECCX08CSRClass::ECCX08CSRClass()
{
}

ECCX08CSRClass::~ECCX08CSRClass()
{
}

#define ASN1_INTEGER           0x02
#define ASN1_BIT_STRING        0x03
#define ASN1_NULL              0x05
#define ASN1_OBJECT_IDENTIFIER 0x06
#define ASN1_PRINTABLE_STRING  0x13
#define ASN1_SEQUENCE          0x30
#define ASN1_SET               0x31

int ECCX08CSRClass::begin(int slot, bool newPrivateKey)
{
  _slot = slot;

  if (newPrivateKey) {
    if (!ECCX08.generatePrivateKey(slot, _publicKey)) {
      return 0;
    }
  } else {
    if (!ECCX08.generatePublicKey(slot, _publicKey)) {
      return 0;
    }
  }

  return 1;
}

String ECCX08CSRClass::end()
{
  int versionLen = versionLength();
  int subjectLen = subjectLength(_countryName,
                                  _stateProvinceName,
                                  _localityName,
                                  _organizationName,
                                  _organizationalUnitName,
                                  _commonName);
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
  appendSubject(_countryName,
                _stateProvinceName,
                _localityName,
                _organizationName,
                _organizationalUnitName,
                _commonName, out);
  out += subjectLen;

  // public key
  appendPublicKey(_publicKey, out);
  out += publicKeyLen;

  // terminator
  *out++ = 0xa0;
  *out++ = 0x00;

  byte csrInfoSha256[64];
  byte signature[64];

  if (!ECCX08.beginSHA256()) {
    return "";
  }

  for (int i = 0; i < (csrInfoHeaderLen + csrInfoLen); i += 64) {
    int chunkLength = (csrInfoHeaderLen + csrInfoLen) - i;

    if (chunkLength > 64) {
      chunkLength = 64;
    }

    if (chunkLength == 64) {
      if (!ECCX08.updateSHA256(&csrInfo[i])) {
        return "";
      }
    } else {
      if (!ECCX08.endSHA256(&csrInfo[i], chunkLength, csrInfoSha256)) {
        return "";
      }
    }
  }

  if (!ECCX08.ecSign(_slot, csrInfoSha256, signature)) {
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

void ECCX08CSRClass::setCountryName(const char *countryName)
{
  _countryName = countryName;
}

void ECCX08CSRClass::setStateProvinceName(const char* stateProvinceName)
{
  _stateProvinceName = stateProvinceName;
}

void ECCX08CSRClass::setLocalityName(const char* localityName)
{
  _localityName = localityName;
}

void ECCX08CSRClass::setOrganizationName(const char* organizationName)
{
  _organizationName = organizationName;
}

void ECCX08CSRClass::setOrganizationalUnitName(const char* organizationalUnitName)
{
  _organizationalUnitName = organizationalUnitName;
}

void ECCX08CSRClass::setCommonName(const char* commonName)
{
  _commonName = commonName;
}

int ECCX08CSRClass::versionLength()
{
  return 3;
}

int ECCX08CSRClass::subjectLength(const String& countryName,
                                  const String& stateProvinceName,
                                  const String& localityName,
                                  const String& organizationName,
                                  const String& organizationalUnitName,
                                  const String& commonName)
{
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

int ECCX08CSRClass::publicKeyLength()
{
  return (2 + 2 + 9 + 10 + 4 + 64);
}

int ECCX08CSRClass::signatureLength(const byte signature[])
{
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

int ECCX08CSRClass::sequenceHeaderLength(int length)
{
  if (length > 255) {
    return 4;
  } else if (length > 127) {
    return 3;
  } else {
    return 2;
  }
}

void ECCX08CSRClass::appendVersion(int version, byte out[])
{
  out[0] = ASN1_INTEGER;
  out[1] = 0x01;
  out[2] = version;
}

void ECCX08CSRClass::appendSubject(const String& countryName,
                                    const String& stateProvinceName,
                                    const String& localityName,
                                    const String& organizationName,
                                    const String& organizationalUnitName,
                                    const String& commonName,
                                    byte out[])
{
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

void ECCX08CSRClass::appendPublicKey(const byte publicKey[], byte out[])
{
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

void ECCX08CSRClass::appendSignature(const byte signature[], byte out[])
{
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

int ECCX08CSRClass::appendName(const String& name, int type, byte out[])
{
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

void ECCX08CSRClass::appendSequenceHeader(int length, byte out[])
{
  *out++ = ASN1_SEQUENCE;
  if (length > 255) {
    *out++ = 0x82;
    *out++ = (length >> 8) & 0xff;
  } else if (length > 127) {
    *out++ = 0x81;
  }
  *out++ = (length) & 0xff;
}

ECCX08CSRClass ECCX08CSR;
