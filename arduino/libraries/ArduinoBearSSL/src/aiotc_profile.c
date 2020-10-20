/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
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

#include "bearssl/inner.h"

/* see bearssl_ssl.h */
void aiotc_client_profile_init(br_ssl_client_context *cc, br_x509_minimal_context *xc, const br_x509_trust_anchor *trust_anchors, size_t trust_anchors_num)
{


  /*
   * The "full" profile supports all implemented cipher suites.
   *
   * Rationale for suite order, from most important to least
   * important rule:
   *
   * -- Don't use 3DES if AES or ChaCha20 is available.
   * -- Try to have Forward Secrecy (ECDHE suite) if possible.
   * -- When not using Forward Secrecy, ECDH key exchange is
   *    better than RSA key exchange (slightly more expensive on the
   *    client, but much cheaper on the server, and it implies smaller
   *    messages).
   * -- ChaCha20+Poly1305 is better than AES/GCM (faster, smaller code).
   * -- GCM is better than CBC.
   * -- AES-128 is preferred over AES-256 (AES-128 is already
   *    strong enough, and AES-256 is 40% more expensive).
   */
  static const uint16_t suites[] = {
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
  };


  int id;

  /*
   * Reset client context and set supported versions from TLS-1.0
   * to TLS-1.2 (inclusive).
   */
  br_ssl_client_zero(cc);
  br_ssl_engine_set_versions(&cc->eng, BR_TLS12, BR_TLS12);

  /*
   * X.509 engine uses SHA-256 to hash certificate DN (for
   * comparisons).
   */
  br_x509_minimal_init(xc, &br_sha256_vtable, trust_anchors, trust_anchors_num);

  /*
   * Set suites and asymmetric crypto implementations. We use the
   * "i31" code for RSA (it is somewhat faster than the "i32"
   * implementation).
   * TODO: change that when better implementations are made available.
   */
  br_ssl_engine_set_suites(&cc->eng, suites, (sizeof suites) / (sizeof suites[0]));
  br_ssl_engine_set_default_ecdsa(&cc->eng);
  br_x509_minimal_set_ecdsa(xc, br_ssl_engine_get_ec(&cc->eng), br_ssl_engine_get_ecdsa(&cc->eng));

  /*
   * Set supported hash functions, for the SSL engine and for the
   * X.509 engine.
   */
  br_ssl_engine_set_hash(&cc->eng, br_sha256_ID, &br_sha256_vtable);
  br_x509_minimal_set_hash(xc, br_sha256_ID, &br_sha256_vtable);

  /*
   * Link the X.509 engine in the SSL engine.
   */
  br_ssl_engine_set_x509(&cc->eng, &xc->vtable);

  /*
   * Set the PRF implementations.
   */
  br_ssl_engine_set_prf_sha256(&cc->eng, &br_tls12_sha256_prf);

  /*
   * Symmetric encryption. We use the "default" implementations
   * (fastest among constant-time implementations).
   */
  br_ssl_engine_set_default_aes_gcm(&cc->eng);
}
