#ifndef _ECCX08_ASN1_H_
#define _ECCX08_ASN1_H_

#include "bearssl/bearssl.h"

size_t
eccX08_sign_asn1(const br_ec_impl *impl,
  const br_hash_class *hf, const void *hash_value,
  const br_ec_private_key *sk, void *sig);

uint32_t
eccX08_vrfy_asn1(const br_ec_impl *impl,
  const void *hash, size_t hash_len,
  const br_ec_public_key *pk,
  const void *sig, size_t sig_len);

#endif
