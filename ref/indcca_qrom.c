#include <string.h>
#include <stdio.h>

#include "indcca_qrom.h"
#include "kem_qrom.h"
#include "randombytes.h"
#include "indcpa.h"
#include "utils.h"
#include "aes256gcm.h"

int pke_qrom_keypair(unsigned char* pk, unsigned char* sk) {
  kem_qrom_keypair(pk, sk);
  return 0;
}

int pke_qrom_enc(unsigned char* m,
                 int len_m,
                 unsigned char* pk,
                 unsigned char* ciphertext_kem,
                 unsigned char* ciphertext_dem,
                 unsigned char* tag,
                 unsigned char* iv,
                 unsigned char* coins) {

  unsigned char K[KYBER_SYMBYTES];
  unsigned char ct[KYBER_INDCPA_BYTES];

  // printf("pk (encaps): ");
  // print_short_key(pk, KYBER_INDCPA_PUBLICKEYBYTES, 10);

  unsigned char* aad = (unsigned char*) "";

  kem_qrom_encaps(pk, coins, K, ct);

  memcpy(ciphertext_kem, ct, KYBER_INDCPA_BYTES);

  // printf("K (encaps): ");
  // print_short_key(K, KYBER_SYMBYTES, 10);
  //
  // printf("coins (encaps): ");
  // print_short_key(coins, KYBER_INDCPA_MSGBYTES, 10);

  // printf("ct (encaps): ");
  // print_short_key(ciphertext_kem, KYBER_INDCPA_BYTES, 10);

  // printf("KYBER_INDCPA_BYTES: %d\n", KYBER_INDCPA_BYTES);
  // printf("sizeof: %zu\n", sizeof(*ciphertext_kem));

  int ret = gcm_encrypt(m, len_m,
                        aad, 0,
                        K,
                        iv, AES_256_IVEC_LENGTH,
                        ciphertext_dem,
                        tag);

  // print_data(m, pk, ciphertext_kem, ciphertext_dem, tag, iv, coins);

  return ret;
}

int pke_qrom_dec(unsigned char* pk, unsigned char* sk,
                 unsigned char* ciphertext_kem,
                 unsigned char* ciphertext_dem,
                 int ciphertext_dem_len,
                 unsigned char* tag,
                 unsigned char* iv,
                 unsigned char* m) {

  unsigned char K_prime[KYBER_SYMBYTES];

  // printf("pk (decaps): ");
  // print_short_key(pk, KYBER_INDCPA_PUBLICKEYBYTES, 10);
  //
  // printf("sk (decaps): ");
  // print_short_key(sk, KYBER_INDCPA_SECRETKEYBYTES, 10);
  //
  // printf("ct (decaps): ");
  // print_short_key(ciphertext_kem, KYBER_INDCPA_BYTES, 10);

  unsigned char* aad = (unsigned char*) "";

  kem_qrom_decaps(pk, sk, ciphertext_kem, K_prime);

  printf("K (decaps): ");
  print_key(K_prime, KYBER_SYMBYTES);

  int ret = gcm_decrypt(ciphertext_dem, ciphertext_dem_len,
                        aad, 0,
                        tag,
                        K_prime,
                        iv, AES_256_IVEC_LENGTH,
                        m);

  return ret;
}
