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

#ifndef _ECCX08_CERT_H_
#define _ECCX08_CERT_H_

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include <Arduino.h>

/******************************************************************************
 * CLASS DECLARATION
 ******************************************************************************/

class ECCX08CertClass {

  public:
    ECCX08CertClass();
    virtual ~ECCX08CertClass();

    int beginCSR(int keySlot, bool newPrivateKey = true);
    String endCSR();

    int beginStorage(int compressedCertSlot, int serialNumberAndAuthorityKeyIdentifierSlot);
    void setSignature(byte signature[]);
    void setIssueYear(int issueYear);
    void setIssueMonth(int issueMonth);
    void setIssueDay(int issueDay);
    void setIssueHour(int issueHour);
    void setExpireYears(int expireYears);
    void setSerialNumber(const byte serialNumber[]);
    void setAuthorityKeyIdentifier(const byte authorityKeyIdentifier[]);
    int endStorage();

    int beginReconstruction(int keySlot, int compressedCertSlot, int serialNumberAndAuthorityKeyIdentifierSlot);
    int endReconstruction();

    byte* bytes();
    int length();

    void setIssuerCountryName(const String& countryName);
    void setIssuerStateProvinceName(const String& stateProvinceName);
    void setIssuerLocalityName(const String& localityName);
    void setIssuerOrganizationName(const String& organizationName);
    void setIssuerOrganizationalUnitName(const String& organizationalUnitName);
    void setIssuerCommonName(const String& commonName);

    void setSubjectCountryName(const String& countryName);
    void setSubjectStateProvinceName(const String& stateProvinceName);
    void setSubjectLocalityName(const String& localityName);
    void setSubjectOrganizationName(const String& organizationName);
    void setSubjectOrganizationalUnitName(const String& organizationalUnitName);
    void setSubjectCommonName(const String& commonName);

  private:
    int versionLength();

    int issuerOrSubjectLength(const String& countryName,
                              const String& stateProvinceName,
                              const String& localityName,
                              const String& organizationName,
                              const String& organizationalUnitName,
                              const String& commonName);

    int publicKeyLength();

    int authorityKeyIdentifierLength(const byte authorityKeyIdentifier[]);

    int signatureLength(const byte signature[]);

    int serialNumberLength(const byte serialNumber[]);

    int sequenceHeaderLength(int length);

    void appendVersion(int version, byte out[]);

    void appendIssuerOrSubject(const String& countryName,
                               const String& stateProvinceName,
                               const String& localityName,
                               const String& organizationName,
                               const String& organizationalUnitName,
                               const String& commonName,
                               byte out[]);

    void appendPublicKey(const byte publicKey[], byte out[]);

    void appendAuthorityKeyIdentifier(const byte authorityKeyIdentifier[], byte out[]);

    void appendSignature(const byte signature[], byte out[]);

    void appendSerialNumber(const byte serialNumber[], byte out[]);

    int appendName(const String& name, int type, byte out[]);

    void appendSequenceHeader(int length, byte out[]);

    int appendDate(int year, int month, int day, int hour, int minute, int second, byte out[]);

    int appendEcdsaWithSHA256(byte out[]);

  private:
    int _keySlot;
    int _compressedCertSlot;
    int _serialNumberAndAuthorityKeyIdentifierSlot;

    String _issuerCountryName;
    String _issuerStateProvinceName;
    String _issuerLocalityName;
    String _issuerOrganizationName;
    String _issuerOrganizationalUnitName;
    String _issuerCommonName;

    String _subjectCountryName;
    String _subjectStateProvinceName;
    String _subjectLocalityName;
    String _subjectOrganizationName;
    String _subjectOrganizationalUnitName;
    String _subjectCommonName;

    byte _temp[108];
    byte* _bytes;
    int _length;
};

#endif /* _ECCX08_CERT_H_ */
