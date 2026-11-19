#include "auth.c"

#define HIGH 2
#define LOW 2
#define BOT 3

unsigned long long LightHardening_len;
unsigned char LightHardening_out[16];
unsigned char LightHardening_k[32];
unsigned char LightHardening_in[];

int annotation_LightHardening_in[] = {HIGH};
int annotation_LightHardening_k[] = {HIGH};
int annotation_LightHardening_out[] = {HIGH};
int annotation_LightHardening_len[] = {LOW};

int annotation_variableLen_LightHardening_in;

void __attribute__((optnone)) LightHardeningEntry(){
    crypto_onetimeauth(LightHardening_out, LightHardening_in, LightHardening_len, LightHardening_k);
}