#include "sha256.c"


#define HIGH 2
#define LOW 2
#define BOT 3


unsigned long long LightHardening_in[];
unsigned char LightHardening_statebytes[32];
unsigned long long LightHardening_inlen;

int annotation_LightHardening_statebytes[] = {LOW};
int annotation_LightHardening_in[] = {HIGH};
int annotation_LightHardening_inlen[] = {LOW};

int annotation_variableLen_LightHardening_in;


void __attribute__((optnone)) LightHardeningEntry(){
    crypto_hashblocks(LightHardening_statebytes, LightHardening_in, LightHardening_inlen);
}