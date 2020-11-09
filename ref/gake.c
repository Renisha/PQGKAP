#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "api.h"
#include "kex.h"
#include "indcpa.h"
#include "randombytes.h"
#include "symmetric.h"
#include "gake.h"

// https://cboard.cprogramming.com/c-programming/101643-mod-negatives.html
int mod(int x, int y){
   int t = x - ((x / y) * y);
   if (t < 0) t += y;
   return t;
}

void xor_keys(uint8_t *x_a, uint8_t *x_b, uint8_t *x_out){

  for (int j = 0; j < KEX_SSBYTES; j++) {
    x_out[j] = x_a[j] ^ x_b[j];
  }
}

void print_key(uint8_t *key, int length) {
  for(int j = 0; j < length; j++){
    printf("%02x", key[j]);
  }
  printf("\n");
}

int check_keys(uint8_t *ka, uint8_t *kb, uint8_t *zero) {
  if(memcmp(ka, kb, KEX_SSBYTES) != 0){
    return 1;
  }

  if(!memcmp(ka, zero, KEX_SSBYTES)){
    return 2;
  }

  return 0;
}

void two_ake(uint8_t *pka, uint8_t *pkb, uint8_t *ska, uint8_t *skb, uint8_t *ka, uint8_t *kb){

  unsigned char eska[CRYPTO_SECRETKEYBYTES];

  unsigned char ake_senda[KEX_AKE_SENDABYTES];
  unsigned char ake_sendb[KEX_AKE_SENDBBYTES];

  unsigned char tk[KEX_SSBYTES];

  // Perform mutually authenticated key exchange
  kex_ake_initA(ake_senda, tk, eska, pkb); // Run by Alice
  kex_ake_sharedB(ake_sendb, kb, ake_senda, skb, pka); // Run by Bob
  kex_ake_sharedA(ka, ake_sendb, tk, eska, ska); // Run by Alice
}

void init_to_zero(uint8_t *key, int length){
  for(int i = 0; i < length; i++){
    key[i] = 0;
  }
}

void print_short_key(uint8_t *key, int length, int show) {
  for (int i = 0; i < show; i++) {
    printf("%02x", key[i]);
  }
  printf("...");
  for (int i = length - show; i < length; i++) {
    printf("%02x", key[i]);
  }
  printf("\n");
}

void concat_masterkey(MasterKey* mk, int num_parties, uint8_t *concat_mk) {
  for (int i = 0; i < num_parties; i++) {
    memcpy(concat_mk + i*KEX_SSBYTES, mk[i], KEX_SSBYTES);
  }
}

void print_party(Party* parties, int i, int num_parties, int show) {
  printf("Party %d\n", i);

  printf("\tPublic key: ");
  print_short_key(parties[i].public_key, CRYPTO_PUBLICKEYBYTES, show);

  printf("\tSecret key: ");
  print_short_key(parties[i].secret_key, CRYPTO_SECRETKEYBYTES, show);

  printf("\tLeft key: ");
  print_short_key(parties[i].key_left, KEX_SSBYTES, show);

  printf("\tRight key: ");
  print_short_key(parties[i].key_right, KEX_SSBYTES, show);

  printf("\tSession id: ");
  print_short_key(parties[i].sid, KEX_SSBYTES, show);

  printf("\tSession key: ");
  print_short_key(parties[i].sk, KEX_SSBYTES, show);

  printf("\tX: \n");
  for (int j = 0; j < num_parties; j++) {
    printf("\t\tX%d: ", j);
    print_short_key(parties[i].xs[j], KEX_SSBYTES, show);
  }

  printf("\tCoins: \n");
  for (int j = 0; j < num_parties; j++) {
    printf("\t\tr%d: ", j);
    print_short_key(parties[i].coins[j], KEX_SSBYTES, show);
  }

  printf("\tCommitments:\n");
  for (int j = 0; j < num_parties; j++) {
    printf("\t\tc%d: ", j);
    print_short_key(parties[i].commitments[j], KYBER_INDCPA_BYTES, show);
  }

  printf("\tMaster Key: \n");
  for (int j = 0; j < num_parties; j++) {
    printf("\t\tk%d: ", j);
    print_short_key(parties[i].masterkey[j], KEX_SSBYTES, show);
  }

  printf("\tPids: \n");
  for (int j = 0; j < num_parties; j++) {
    printf("\t\tpid%d: %s\n", j, *parties[i].pids[j]);
  }
}

void init_parties(Party* parties, int num_parties) {
  for (int i = 0; i < num_parties; i++) {
    parties[i].commitments = malloc(sizeof(Commitment) * num_parties);
    parties[i].masterkey = malloc(sizeof(MasterKey) * num_parties);
    parties[i].pids = malloc(sizeof(Pid) * num_parties);
    parties[i].coins = malloc(sizeof(Coins) * num_parties);
    parties[i].xs = malloc(sizeof(X) * num_parties);

    for (int j = 0; j < num_parties; j++) {
      char* pid = malloc(20*sizeof(char));
      sprintf(pid, "%s %d", "Party", j);
      *(parties[i].pids)[j] = pid;
    }

    init_to_zero(parties[i].sid, KEX_SSBYTES);
    init_to_zero(parties[i].sk, KEX_SSBYTES);

    crypto_kem_keypair(parties[i].public_key,
                       parties[i].secret_key);

  }
}

void print_parties(Party* parties, int num_parties, int show) {
  for (int i = 0; i < num_parties; i++) {
    print_party(parties, i, num_parties, show);
  }
}

void free_parties(Party* parties, int num_parties) {
  for (int i = 0; i < num_parties; i++) {
    free(parties[i].commitments);
    free(parties[i].masterkey);
    free(parties[i].pids);
    free(parties[i].coins);
    free(parties[i].xs);
  }
  free(parties);
}

void compute_sk_sid(Party* parties, int num_parties) {
  for (int i = 0; i < num_parties; i++) {
    unsigned char mki[KEX_SSBYTES*num_parties];

    // Concat master key
    concat_masterkey(parties[i].masterkey, num_parties, mki);

    unsigned char sk_sid[2*KEX_SSBYTES];

    hash_g(sk_sid, mki, 2*KEX_SSBYTES);

    printf("sk_sid: ");
    print_short_key(sk_sid, 2*KEX_SSBYTES, 10);

    memcpy(parties[i].sk, sk_sid, KEX_SSBYTES);
    memcpy(parties[i].sid, sk_sid + KEX_SSBYTES, KEX_SSBYTES);
  }
}

void compute_masterkey(Party* parties, int num_parties) {

  for (int i = 0; i < num_parties; i++) {
    memcpy(parties[i].masterkey[i],
           parties[i].key_left, KEX_SSBYTES);

    for (int j = 1; j < num_parties; j++) {
      MasterKey mk;
      memcpy(mk, parties[i].key_left, KEX_SSBYTES);
      for (int k = 0; k < j; k++) {
        xor_keys(mk, parties[i].xs[mod(i-k-1,num_parties)], mk);
      }

      memcpy(parties[i].masterkey[mod(i-j, num_parties)],
             mk, KEX_SSBYTES);

    }
  }
}

int check_commitments(Party* parties, int i, int num_parties) {
  for (int j = 0; j < num_parties; j++) {
    Commitment ci_check;

    indcpa_enc(ci_check,
               parties[i].xs[j],
               parties[j].public_key,
               parties[i].coins[j]);

    int res_check = memcmp(ci_check, parties[i].commitments[j], KYBER_INDCPA_BYTES);
    if (res_check != 0) {
      return 1;
    }
  }
  return 0;
}

int check_xs(Party* parties, int i, int num_parties) {
  unsigned char zero[KEX_SSBYTES];

  for(int j = 0; j < KEX_SSBYTES; j++){
    zero[j] = 0;
  }

  X check;
  memcpy(check, parties[i].xs[0], KEX_SSBYTES);
  for (int j = 0; j < num_parties - 1; j++) {
    xor_keys(parties[i].xs[j+1], check, check);
  }

  int res = memcmp(check, zero, KEX_SSBYTES);
  if (res != 0) {
    return 1;
  }
  return 0;
}

void compute_xs_commitments(Party* parties, int num_parties) {
  for (int i = 0; i < num_parties; i++) {
    X xi;
    Coins ri;
    Commitment ci;

    xor_keys(parties[i].key_right, parties[i].key_left, xi);
    randombytes(ri, KYBER_SYMBYTES);
    indcpa_enc(ci, xi, parties[i].public_key, ri);

    for (int j = 0; j < num_parties; j++) {
      memcpy(parties[j].xs[i], &xi, KEX_SSBYTES);
      memcpy(parties[j].coins[i], &ri, KYBER_SYMBYTES);
      memcpy(parties[j].commitments[i], &ci, KYBER_INDCPA_BYTES);
    }
  }
}

void compute_left_right_keys(Party* parties, int num_parties) {
  for (int i = 0; i < num_parties; i++) {
    int right = mod(i+1, num_parties);
    int left = mod(i-1, num_parties);

    two_ake(parties[i].public_key, parties[right].public_key,
            parties[i].secret_key, parties[right].secret_key,
            parties[i].key_right,   parties[right].key_left);

    two_ake(parties[i].public_key, parties[left].public_key,
            parties[i].secret_key, parties[left].secret_key,
            parties[i].key_left,   parties[left].key_right);
  }
}
