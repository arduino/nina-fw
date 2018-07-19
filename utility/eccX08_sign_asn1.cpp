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
#define ORDER_LEN   ((BR_MAX_EC_SIZE + 7) >> 3)

size_t
eccX08_sign_asn1(const br_ec_impl * /*impl*/,
  const br_hash_class * /*hf*/, const void *hash_value,
  const br_ec_private_key *sk, void *sig)
{
  unsigned char rsig[(ORDER_LEN << 1) + 12];
  size_t sig_len;

  if (sk->curve != BR_EC_secp256r1) {
    return 0;
  }

  if (!ECCX08.ecSign((int)(sk->x), (const uint8_t*)hash_value, (uint8_t*)rsig)) {
    return 0;
  }
  sig_len = 64;

  sig_len = br_ecdsa_raw_to_asn1(rsig, sig_len);
  memcpy(sig, rsig, sig_len);
  return sig_len;
}
