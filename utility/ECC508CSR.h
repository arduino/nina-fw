#ifndef _ECC508_CSR_H_
#define _ECC508_CSR_H_

#include <Arduino.h>

class ECC508CSRClass {
public:
  ECC508CSRClass();
  virtual ~ECC508CSRClass();

  int begin(int slot, bool newPrivateKey = true);
  String end();

  void setCountryName(const char *countryName);
  void setCountryName(const String& countryName) { setCountryName(countryName.c_str()); }

  void setStateProvinceName(const char* stateProvinceName);
  void setStateProvinceName(const String& stateProvinceName) { setStateProvinceName(stateProvinceName.c_str()); }

  void setLocalityName(const char* localityName);
  void setLocalityName(const String& localityName) { setLocalityName(localityName.c_str()); }

  void setOrganizationName(const char* organizationName);
  void setOrganizationName(const String& organizationName) { setOrganizationName(organizationName.c_str()); }

  void setOrganizationalUnitName(const char* organizationalUnitName);
  void setOrganizationalUnitName(const String& organizationalUnitName) { setOrganizationalUnitName(organizationalUnitName.c_str()); }

  void setCommonName(const char* commonName);
  void setCommonName(const String& commonName) { setCommonName(commonName.c_str()); }

private:
  int encodeCsrInfo(byte buffer[]);
  int encodeEcdsaSignature(const byte signature[], byte buffer[]);

  int encodeName(const char* name, int length, int type, byte buffer[]);
  int encodePublicKey(byte buffer[]);

private:
  int _slot;

  String _countryName;
  String _stateProvinceName;
  String _localityName;
  String _organizationName;
  String _organizationalUnitName;
  String _commonName;

  byte _publicKey[64];
};

extern ECC508CSRClass ECC508CSR;

#endif
