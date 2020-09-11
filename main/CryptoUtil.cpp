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

#include "CryptoUtil.h"

/******************************************************************************
 * PUBLIC MEMBER FUNCTIONS
 ******************************************************************************/

bool CryptoUtil::readDeviceId(ECCX08Class & eccx08, String & device_id, ECCX08Slot const device_id_slot)
{
  byte device_id_bytes[72] = {0};
  
  if (eccx08.readSlot(static_cast<int>(device_id_slot), device_id_bytes, sizeof(device_id_bytes))) {
   device_id = String(reinterpret_cast<char *>(device_id_bytes));
   return true;
  }
  else
  {
   return false;
  }
}

bool CryptoUtil::reconstructCertificate(ECCX08CertClass & cert, String const & device_id, ECCX08Slot const key, ECCX08Slot const compressed_certificate, ECCX08Slot const serial_number_and_authority_key)
{
  if (cert.beginReconstruction(static_cast<int>(key), static_cast<int>(compressed_certificate), static_cast<int>(serial_number_and_authority_key)))
  {
    cert.setSubjectCommonName(device_id);
    cert.setIssuerCountryName("US");
    cert.setIssuerOrganizationName("Arduino LLC US");
    cert.setIssuerOrganizationalUnitName("IT");
    cert.setIssuerCommonName("Arduino");
    return cert.endReconstruction();
  }
  else
  {
   return false;
  }
}
