#include "ecc508_asn1.h"

#include "ECC508.h"

#define BR_MAX_EC_SIZE   528
#define FIELD_LEN   ((BR_MAX_EC_SIZE + 7) >> 3)

uint32_t
ecc508_vrfy_asn1(const br_ec_impl * /*impl*/,
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
  if (!ECC508.ecdsaVerify((const uint8_t*)hash, (const uint8_t*)rsig, (const uint8_t*)&pk->q[1])) {
    return 0;
  }

  return 1;
}
