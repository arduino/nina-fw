#ifndef _ECC508_ASN1_H_
#define _ECC508_ASN1_H_

#include "bearssl/bearssl.h"

uint32_t
ecc508_vrfy_asn1(const br_ec_impl *impl,
  const void *hash, size_t hash_len,
  const br_ec_public_key *pk,
  const void *sig, size_t sig_len);

#endif
