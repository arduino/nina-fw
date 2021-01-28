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

#ifndef ARDUINO_IOT_CLOUD_UTILITY_CRYPTO_CRYPTO_UTIL_H_
#define ARDUINO_IOT_CLOUD_UTILITY_CRYPTO_CRYPTO_UTIL_H_

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include <Arduino.h>
#include <ArduinoECCX08.h>
#include "ECCX08Cert.h"

/******************************************************************************
   TYPEDEF
 ******************************************************************************/

enum class ECCX08Slot : int
{
  Key                                   = 0,
  CompressedCertificate                 = 10,
  SerialNumberAndAuthorityKeyIdentifier = 11,
  DeviceId                              = 12
};

/******************************************************************************
 * CLASS DECLARATION
 ******************************************************************************/

class CryptoUtil
{
public:

  static bool readDeviceId(ECCX08Class & eccx08, String & device_id, ECCX08Slot const device_id_slot);
  static bool reconstructCertificate(ECCX08CertClass & cert, String const & device_id, ECCX08Slot const key, ECCX08Slot const compressed_certificate, ECCX08Slot const serial_number_and_authority_key);


private:

  CryptoUtil() { }
  CryptoUtil(CryptoUtil const & other) { }

};

#endif /* ARDUINO_IOT_CLOUD_UTILITY_CRYPTO_CRYPTO_UTIL_H_ */
