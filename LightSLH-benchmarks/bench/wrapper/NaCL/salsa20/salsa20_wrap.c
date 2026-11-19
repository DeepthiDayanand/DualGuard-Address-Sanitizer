#include "core.c"
#include <stdint.h>

#define HIGH 2
#define LOW 2
#define BOT 3

uint8_t LightHardening_out[64];
uint8_t LightHardening_in[16];
uint8_t LightHardening_c[16];
uint8_t LightHardening_k[32];


int annotation_LightHardening_c[] = {LOW};
int annotation_LightHardening_out[] = {LOW};
int annotation_LightHardening_in[] = {HIGH};
int annotation_LightHardening_k[] = {HIGH};


void __attribute__((optnone)) LightHardeningEntry(){
    crypto_core(LightHardening_out, LightHardening_in, LightHardening_k,
                   LightHardening_c);
}