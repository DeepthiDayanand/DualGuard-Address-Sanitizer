#define HIGH 2
#define LOW 2
#define BOT 3
#include <stdint.h>

int PQCLEAN_KYBER512_CLEAN_crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);


unsigned char LightHardening_ss[1632];
unsigned char LightHardening_pk[800];
unsigned char LightHardening_ct[736]; 

int annotation_LightHardening_ss[] = {HIGH};
int annotation_LightHardening_pk[] = {LOW};
int annotation_LightHardening_ct[] = {HIGH};

void LightHardeningEntry(){
    PQCLEAN_KYBER512_CLEAN_crypto_kem_enc(LightHardening_ct, LightHardening_ss, LightHardening_pk);
}