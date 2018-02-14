#include "eccX08_asn1.h"

#include "ECCX08.h"

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
