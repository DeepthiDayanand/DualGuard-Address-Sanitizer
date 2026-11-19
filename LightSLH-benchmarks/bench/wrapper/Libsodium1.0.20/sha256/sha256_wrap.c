#include "common.h"
#include "sha256.c"


#define HIGH 2
#define LOW 2
#define BOT 3

crypto_hash_sha256_state LightHardening_state;
uint8_t LightHardening_in[];
unsigned long long LightHardening_inlen;

int annotation_LightHardening_state[] = {LOW,LOW,LOW};
int annotation_LightHardening_in[] = {HIGH};
int annotation_LightHardening_inlen[] = {LOW};

int annotation_variableLen_LightHardening_in;


void __attribute__((optnone)) LightHardeningEntry(){
    crypto_hash_sha256_update(&LightHardening_state, LightHardening_in, LightHardening_inlen);
}