#include "ec/curve25519.c"

#define HIGH 2
#define LOW 2
#define BOT 3

uint8_t LightHardening_out_shared_key[32];
uint8_t LightHardening_private_key[32];
uint8_t LightHardening_peer_public_value[32];

int annotation_LightHardening_out_shared_key[] = {HIGH};
int annotation_LightHardening_private_key[] = {HIGH};
int annotation_LightHardening_peer_public_value[] = {LOW};

void __attribute__((optnone)) LightHardeningEntry(){
    ossl_x25519(LightHardening_out_shared_key, LightHardening_private_key, LightHardening_peer_public_value);
}