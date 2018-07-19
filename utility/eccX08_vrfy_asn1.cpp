/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
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

#include "eccX08_asn1.h"

#include <ArduinoECCX08.h>

#define BR_MAX_EC_SIZE   528
#define FIELD_LEN   ((BR_MAX_EC_SIZE + 7) >> 3)

uint32_t
eccX08_vrfy_asn1(const br_ec_impl * /*impl*/,
  const void *hash, size_t hash_len,
  const br_ec_public_key *pk,
  const void *sig, size_t sig_len)
{
  /*
   * We use a double-sized buffer because a malformed ASN.1 signature
   * may trigger a size expansion when converting to "raw" format.
   */
  unsigned char rsig[(FIELD_LEN << 2) + 24];

  if (sig_len > ((sizeof rsig) >> 1)) {
    return 0;
  }

  memcpy(rsig, sig, sig_len);
  sig_len = br_ecdsa_asn1_to_raw(rsig, sig_len);

  if (hash_len != 32 || pk->curve != BR_EC_secp256r1 || pk->qlen != 65 || sig_len != 64) {
    return 0;
  }

  // TODO: understand why the public key is &pk->q[1] instead of &pk->q[0] ...
  if (!ECCX08.ecdsaVerify((const uint8_t*)hash, (const uint8_t*)rsig, (const uint8_t*)&pk->q[1])) {
    return 0;
  }

  return 1;
}
