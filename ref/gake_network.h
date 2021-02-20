#include "gake.h"
#include "io.h"

void compute_masterkey_i(Party* party, int num_parties, int index);
void compute_xs_commitment(Party* party, int index);
void compute_sk_sid_i(Party* party, int num_parties);
void init_party(Party* party, int num_parties, ip_t* ips, keys_t* keys);
void set_m1(Party* party, int index, uint8_t* message);
void set_m2(Party* party, int index, uint8_t* message);
int check_m1_received(Party* party, int num_parties);
int is_zero_xs_coins(X* xs, Coins* coins);
int check_m2_received(Party* party, int num_parties);
int check_xs_i(Party* party, int num_parties);
int check_commitments_i(Party* party, int num_parties, ca_public* data);
void read_n_bytes(int socket, int x, unsigned char* buffer);
void free_party(Party* party, int num_parties);