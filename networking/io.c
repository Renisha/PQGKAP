#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "io.h"
#include "../ref/aes256gcm.h"
#include "../ref/randombytes.h"

#ifndef MESSAGE_LENGTH
#define MESSAGE_LENGTH 1024
#endif

int read_keys(char* filename, keys_t* data) {
  FILE* fp;
  fp = fopen(filename, "rb");
  if (fp == NULL)
    return 1;

  if (fread(data, sizeof(keys_t), 1, fp)) {
    return 1;
  }
  fclose(fp);
  return 0;
}

int read_ca_data(char* filename, int num_parties, ca_public* data) {
  FILE* fp;
  fp = fopen(filename, "rb");
  if (fp == NULL)
    return 1;

  if (fread(data, sizeof(ca_public), num_parties, fp) != num_parties) {
    return 1;
  }
  fclose(fp);
  return 0;
}

int get_pk(char* ip_str, uint8_t* pk, ca_public* data, int length) {
  ip_t ip;
  set_ip(ip_str, &ip);
  for (int i = 0; i < length; i++) {
    if(memcmp(data[i].ip, ip, sizeof(ip_t)) == 0) {
      memcpy(pk, data[i].public_key, CRYPTO_PUBLICKEYBYTES);
      return 0;
    }
  }
  return 1;
}

void set_ip(char* ip_str, ip_t* ip) {
  short j = 0;
  char* token = strtok(ip_str, ".");
  ip_t out;
  for (short j = 0; j < 4; j++) {
    uint8_t value = (uint8_t) atoi(token);
    out[j] = value;
    token = strtok(NULL, ".");
  }
  memcpy(ip, out, sizeof(ip_t));
}

void ip_to_str(ip_t ip, char* ip_str) {
  sprintf(ip_str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

void set_message(uint8_t* iv, uint8_t* tag, char* ct, int ct_len, uint8_t* message) {
  memcpy(message, iv, AES_256_IVEC_LENGTH);
  memcpy(message + AES_256_IVEC_LENGTH, tag, AES_256_GCM_TAG_LENGTH);
  memcpy(message + AES_256_IVEC_LENGTH + AES_256_GCM_TAG_LENGTH, &ct_len, sizeof(int));
  memcpy(message + AES_256_IVEC_LENGTH + AES_256_GCM_TAG_LENGTH + sizeof(int), ct, ct_len);
}

void unset_message(uint8_t* message, uint8_t* iv, uint8_t* tag, char* ct, int *ct_len) {
  memcpy(iv, message, AES_256_IVEC_LENGTH);
  memcpy(tag, message + AES_256_IVEC_LENGTH, AES_256_GCM_TAG_LENGTH);
  memcpy(ct_len, message + AES_256_IVEC_LENGTH + AES_256_GCM_TAG_LENGTH, sizeof(int));
  memcpy(ct, message + AES_256_IVEC_LENGTH + AES_256_GCM_TAG_LENGTH + sizeof(int), *ct_len);
}

void encrypt(char* m, uint8_t* k, uint8_t* message) {

  unsigned char* aad = (unsigned char *) "";
  int aad_len = strlen((char *) aad);

  unsigned char iv[AES_256_IVEC_LENGTH];
  unsigned char ct[MESSAGE_LENGTH];
  unsigned char tag[AES_256_GCM_TAG_LENGTH];
  bzero(ct, MESSAGE_LENGTH);
  randombytes(iv, AES_256_IVEC_LENGTH);
  int ct_len = gcm_encrypt(m, strlen(m), aad, aad_len, k, iv, ct, tag);
  set_message(iv, tag, ct, ct_len, message);
}

void decrypt(uint8_t* message, uint8_t* k, char* m) {
  unsigned char iv[AES_256_IVEC_LENGTH];
  unsigned char tag[AES_256_GCM_TAG_LENGTH];
  unsigned char ct[MESSAGE_LENGTH];
  unsigned char plaintext[MESSAGE_LENGTH];
  int ct_len;

  unset_message(message, iv, tag, ct, &ct_len);

  unsigned char* aad = (unsigned char *) "";
  int aad_len = strlen((char *) aad);

  gcm_decrypt(ct, ct_len, aad, aad_len, tag, k, iv, AES_256_IVEC_LENGTH, m);
}