#include "chacha/chacha_enc.c"

#define HIGH 2
#define LOW 2
#define BOT 3

unsigned char LightHardening_out[];
unsigned char LightHardening_inp[];
size_t LightHardening_len;
unsigned int LightHardening_key[8];
unsigned int LightHardening_counter[4];

int annotation_LightHardening_out[] = {LOW};
int annotation_LightHardening_inp[] = {LOW};
int annotation_LightHardening_len[] = {LOW};
int annotation_LightHardening_key[] = {HIGH};
int annotation_LightHardening_counter[] = {LOW};

int annotation_variableLen_LightHardening_out;
int annotation_variableLen_LightHardening_inp;


void __attribute__((optnone)) LightHardeningEntry(){
    ChaCha20_ctr32(LightHardening_out, LightHardening_inp, LightHardening_len,
                   LightHardening_key, LightHardening_counter);
}