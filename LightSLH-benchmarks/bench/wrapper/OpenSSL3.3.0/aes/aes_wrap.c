#include "aes/aes_core.c"
#include <openssl/aes.h>

#define HIGH 2
#define LOW 2
#define BOT 3



unsigned char LightHardening_in[16];
unsigned char LightHardening_out[16];
AES_KEY LightHardening_key;

int annotation_LightHardening_in[] = {LOW};
int annotation_LightHardening_out[] = {LOW};
int annotation_LightHardening_key[] = {HIGH, BOT};

void LightHardeningEntry(){
    LightHardening_key.rounds = 10;
    AES_encrypt(LightHardening_in, LightHardening_out, &LightHardening_key);
}