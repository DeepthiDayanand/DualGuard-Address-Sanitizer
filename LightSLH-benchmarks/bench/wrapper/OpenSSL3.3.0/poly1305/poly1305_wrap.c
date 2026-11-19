#include "poly1305/poly1305.c"

#define HIGH 2
#define LOW 2
#define BOT 3

POLY1305 LightHardening_ctx;
size_t LightHardening_len;
unsigned char LightHardening_inp[];

int annotation_LightHardening_ctx[] = {HIGH, HIGH, HIGH, LOW, LOW, LOW};
int annotation_LightHardening_inp[] = {LOW};
int annotation_LightHardening_len[] = {LOW};

int annotation_variableLen_LightHardening_inp;

void __attribute__((optnone)) LightHardeningEntry(){
    Poly1305_Update(&LightHardening_ctx, LightHardening_inp, LightHardening_len);
}