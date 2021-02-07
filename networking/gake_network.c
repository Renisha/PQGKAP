#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "io.h"
#include "common.h"
#include "../ref/utils.h"
#include "../ref/gake.h"

void compute_masterkey_i(Party* party, int num_parties, int index) {

  memcpy(party->masterkey[index],
         party->key_left, KEX_SSBYTES);

  for (int j = 1; j < num_parties; j++) {
    MasterKey mk;
    memcpy(mk, party->key_left, KEX_SSBYTES);
    for (int k = 0; k < j; k++) {
      xor_keys(mk, party->xs[mod(index-k-1,num_parties)], mk);
    }

    memcpy(party->masterkey[mod(index-j, num_parties)],
           mk, KEX_SSBYTES);

  }
}

void compute_xs_commitment(Party* party, int index) {
    X xi;
    Coins ri;
    Commitment ci;

    unsigned char msg[KEX_SSBYTES + sizeof(int)];
    init_to_zero(msg, KEX_SSBYTES + sizeof(int));
    char buf_int[sizeof(int)];
    init_to_zero((unsigned char*) buf_int, KEX_SSBYTES + sizeof(int));
    itoa(index, buf_int);

    xor_keys(party->key_right, party->key_left, xi);
    randombytes(ri, COMMITMENTCOINSBYTES);

    memcpy(msg, &xi, KEX_SSBYTES);
    memcpy(msg + KEX_SSBYTES, &buf_int, sizeof(int));
    commit(party->public_key, msg, DEM_LEN, ri, &ci);

    memcpy(party->xs[index], &xi, KEX_SSBYTES);
    memcpy(party->coins[index], &ri, COMMITMENTCOINSBYTES);
    party->commitments[index] = ci;
}

void compute_sk_sid_i(Party* party, int num_parties) {

    unsigned char mki[(KEX_SSBYTES + PID_LENGTH*sizeof(char))*num_parties];

  // Concat master key
  concat_masterkey(party->masterkey, party->pids, num_parties, mki);

  unsigned char sk_sid[2*KEX_SSBYTES];

  hash_g(sk_sid, mki, 2*KEX_SSBYTES);

  memcpy(party->sk, sk_sid, KEX_SSBYTES);
  memcpy(party->sid, sk_sid + KEX_SSBYTES, KEX_SSBYTES);

  party->acc = 1;
  party->term = 1;
}

void init_party(Party* party, int num_parties, ip_t* ips, keys_t* keys) {
  party->commitments = malloc(sizeof(Commitment) * num_parties);
  party->masterkey = malloc(sizeof(MasterKey) * num_parties);
  party->pids = malloc(sizeof(Pid) * num_parties);
  party->coins = malloc(sizeof(Coins) * num_parties);
  party->xs = malloc(sizeof(X) * num_parties);

  for (int j = 0; j < num_parties; j++) {
    char pid[PID_LENGTH];
    ip_to_str(ips[j], pid);
    sprintf(pid, "%s", pid);
    memcpy(party->pids[j], pid, PID_LENGTH);
  }

  for (int j = 0; j < num_parties; j++) {
    init_to_zero(party->commitments[j].ciphertext_kem, KYBER_CIPHERTEXTBYTES);
    init_to_zero(party->commitments[j].ciphertext_dem, DEM_LEN);
    init_to_zero(party->commitments[j].tag, AES_256_GCM_TAG_LENGTH);
    init_to_zero(party->coins[j], COMMITMENTCOINSBYTES);
    init_to_zero(party->masterkey[j], KEX_SSBYTES);
    init_to_zero(party->xs[j], KEX_SSBYTES);
  }

  init_to_zero(party->sid, KEX_SSBYTES);
  init_to_zero(party->sk, KEX_SSBYTES);
  init_to_zero(party->key_left, KEX_SSBYTES);
  init_to_zero(party->key_right, KEX_SSBYTES);

  memcpy(party->public_key, keys->public_key, CRYPTO_PUBLICKEYBYTES);
  memcpy(party->secret_key, keys->secret_key, CRYPTO_SECRETKEYBYTES);

  party->acc = 0;
  party->term = 0;
}


int main(int argc, char* argv[]) {

  if (argc != 5) {
    printf("Usage: server <private key file> <ca keys file> <parties ips file> <ip>\n");
    exit(1);
  }

  int NUM_PARTIES = count_lines(argv[3]);
  printf("IPs read: %d\n", NUM_PARTIES);

  keys_t keys;
  ca_public* data = (ca_public*) malloc(NUM_PARTIES*sizeof(ca_public));

  ip_t* ips = (ip_t*) malloc(NUM_PARTIES*sizeof(ip_t));

  read_ips(argv[3], ips);
  int index = -1;
  if ((index = get_index(ips, NUM_PARTIES, argv[4])) == -1) {
    printf("Index not found!\n");
    return 1;
  }

  printf("index: %d\n", index);

  read_keys(argv[1], &keys);
  printf("pk: ");
  print_short_key(keys.public_key, CRYPTO_PUBLICKEYBYTES, 10);
  printf("sk: ");
  print_short_key(keys.secret_key, CRYPTO_SECRETKEYBYTES, 10);

  read_ca_data(argv[2], NUM_PARTIES, data);

  printf("Reading CA data...\n");
  printf("---------------------------------------------------\n");
  for (int i = 0; i < NUM_PARTIES; i++) {
    char ip_str[17];
    ip_to_str(data[i].ip, ip_str);
    printf("ip: %s\n", ip_str);
    printf("pk: ");
    print_short_key(data[i].public_key, CRYPTO_PUBLICKEYBYTES, 10);
    printf("---------------------------------------------------\n");
  }

  Party party;

  init_party(&party, NUM_PARTIES, ips, &keys);
  print_party(&party, 0, NUM_PARTIES, 10);

  int left = mod(index - 1, NUM_PARTIES);
  int right = mod(index + 1, NUM_PARTIES);
  printf("left: %s\n", (char*) party.pids[left]);
  printf("right: %s\n", (char*) party.pids[right]);

  char ip_str[17];
  unsigned char pk_left[CRYPTO_PUBLICKEYBYTES];
  unsigned char pk_right[CRYPTO_PUBLICKEYBYTES];
  get_pk((char*) party.pids[left], pk_left, data, NUM_PARTIES);
  get_pk((char*) party.pids[right], pk_right, data, NUM_PARTIES);

  printf("pk (l): ");
  print_short_key(pk_left, CRYPTO_PUBLICKEYBYTES, 10);

  printf("pk (r): ");
  print_short_key(pk_right, CRYPTO_PUBLICKEYBYTES, 10);

  // start_server(party.public_key, party.secret_key, pk_left, party.key_left);
  // print_party(&party, 0, NUM_PARTIES, 10);

  // start_client(party.public_key, party.secret_key, pk_right, (char*) party.pids[right], party.key_left);

  printf("left: %s\n", (char*) party.pids[left]);
  printf("right: %s\n", (char*) party.pids[right]);

  int pi_d;
  int pid;
  int fd1[2], fd2[2];

  pipe(fd1);
  pipe(fd2);

  // Round 1-2
  pi_d = fork();
  if(pi_d == 0){
    printf("Child Process B:\n\tpid:%d\n\tppid:%d\n",getpid(),getppid());

    struct sockaddr_in serveraddress, client;
    socklen_t length;
    int sockert_file_descriptor, connection, bind_status, connection_status;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1){
      printf("Socket creation failed!\n");
      exit(1);
    } else {
      printf("Socket fd: %d\n", fd);
    }

    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
      printf("setsockopt(SO_REUSEADDR) failed!\n");
      exit(1);
    }

    bzero(&serveraddress, sizeof(serveraddress));
    serveraddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddress.sin_port = htons(PORT);
    serveraddress.sin_family = AF_INET;

    bind_status = bind(fd, (SA*)&serveraddress, sizeof(serveraddress));

    if(bind_status == -1){
      printf("Socket binding failed.!\n");
      exit(1);
    }

    connection_status = listen(fd, 5);

    if(connection_status == -1) {
      printf("Socket is unable to listen for new connections!\n");
      exit(1);
    } else {
      printf("Server is listening for new connection: \n");
    }

    length = sizeof(client);
    connection = accept(fd, (SA*)&client, &length);

    if(connection == -1){
      printf("Server is unable to accept the data from client!\n");
      exit(1);
    }

    unsigned char ake_sendb[KEX_AKE_SENDBBYTES];
    unsigned char ake_senda[KEX_AKE_SENDABYTES];

    bzero(ake_senda, sizeof(ake_senda));
    bzero(party.key_left, sizeof(party.key_left));

    int bytes = 0;

    read(connection, ake_senda, sizeof(ake_senda));
    printf("[C] ake_senda: ");
    print_short_key(ake_senda, KEX_AKE_SENDABYTES, 10);

    kex_ake_sharedB(ake_sendb, party.key_left, ake_senda, party.secret_key, pk_left);
    printf("[S] ake_sendb: ");
    print_short_key(ake_sendb, KEX_AKE_SENDABYTES, 10);

    // Send second message
    bytes = write(connection, ake_sendb, sizeof(ake_sendb));
    if(bytes >= 0){
      printf("Data sent successfully!\n");
    }

    printf("[S] key (l): ");
    print_key(party.key_left, KEX_SSBYTES);

    close(fd);

    write(fd1[1], party.key_left, sizeof(party.key_left));
    close(fd1[1]);
    exit(0);
  }

  if(pi_d > 0){
    pid = fork();
    if(pid > 0){
      printf("\nParent Process:\n\tpid:%d\n\tppid :%d\n",getpid(),getppid());
      close(fd1[1]);
      close(fd2[1]);
      read(fd1[0], party.key_left, sizeof(party.key_left));
      printf("key (l): ");
      print_key(party.key_left, KEX_SSBYTES);

      read(fd2[0], party.key_right, sizeof(party.key_right));
      printf("key (r): ");
      print_key(party.key_right, KEX_SSBYTES);

      print_party(&party, 0, NUM_PARTIES, 10);
      close(fd1[0]);
      close(fd2[0]);
    }
    else if(pid == 0){
      printf("Child Process A:\n\tpid:%d\n\tppid:%d\n",getpid(),getppid());

      struct sockaddr_in serveraddress;

      int fd = socket(AF_INET, SOCK_STREAM, 0);
      if(fd == -1){
        printf("Creation of Socket failed.!\n");
        exit(1);
      }

      bzero(&serveraddress, sizeof(serveraddress));
      serveraddress.sin_addr.s_addr = inet_addr((char*) party.pids[right]);
      serveraddress.sin_port = htons(PORT);
      serveraddress.sin_family = AF_INET;

      int connection;
      do {
        connection = connect(fd, (SA*)&serveraddress, sizeof(serveraddress));

        if(connection == -1){
          printf("Waiting for the server %s to be ready...\n", (char*) party.pids[right]);
          sleep(3);
        }
      } while(connection == -1);

      unsigned char eska[CRYPTO_SECRETKEYBYTES];
      unsigned char ake_senda[KEX_AKE_SENDABYTES];
      unsigned char ake_sendb[KEX_AKE_SENDBBYTES];
      unsigned char tk[KEX_SSBYTES];

      kex_ake_initA(ake_senda, tk, eska, pk_right);
      printf("[C] ake_senda: ");
      print_short_key(ake_senda, KEX_AKE_SENDABYTES, 10);

      ssize_t bytes = 0;

      bytes = write(fd, ake_senda, sizeof(ake_senda));

      // if(bytes >= 0){
      //   printf("Data sent successfully!\n");
      // }

      read(fd, ake_sendb, sizeof(ake_sendb));
      printf("[S] ake_sendb: ");
      print_short_key(ake_sendb, KEX_AKE_SENDABYTES, 10);

      kex_ake_sharedA(party.key_right, ake_sendb, tk, eska, party.secret_key);
      printf("[C] key: ");
      print_key(party.key_right, KEX_SSBYTES);

      write(fd2[1], &party.key_right, sizeof(party.key_right));
      close(fd2[1]);
      exit(0);
    }
  }
  pid_t wpid;
  int status = 0;
  while ((wpid = wait(&status)) > 0); // Wait to finish child processes

  printf("Después de terminar\n");
  print_party(&party, 0, NUM_PARTIES, 10);

  // Round 3
  compute_xs_commitment(&party, index);
  print_party(&party, 0, NUM_PARTIES, 10);

  // Todo: broadcast M^1_i

  // Round 4

  // Todo: broadcast M^2_i

  // int res = check_xs(&party, index, NUM_PARTIES); // Check Xi
  // int result = check_commitments(&party, index, NUM_PARTIES); // Check commitments
  //
  // if (res == 0) {
  //   printf("Xi are zero!\n");
  // } else {
  //   printf("\t\tXi are not zero!\n");
  //   party.acc = 0;
  //   party.term = 1;
  //   // return 1;
  // }
  //
  // if (result == 0) {
  //   printf("\t\tCommitments are correct!\n");
  // } else {
  //   printf("\t\tCommitments are not correct!\n");
  //   party.acc = 0;
  //   party.term = 1;
  //   // return 1;
  // }

  // compute_masterkey_i(&party, NUM_PARTIES, index);
  // print_party(&party, 0, NUM_PARTIES, 10);
  //
  // compute_sk_sid_i(&party, NUM_PARTIES);
  // print_party(&party, 0, NUM_PARTIES, 10);

  free(ips);
  free(data);
  return 0;
}
