#define HIGH 2
#define LOW 2
#define BOT 3

void PQCLEAN_MCELIECE348864_CLEAN_encrypt(unsigned char *s, const unsigned char *pk, unsigned char *e);


unsigned char LightHardening_s[96];
unsigned char LightHardening_pk[261120];
unsigned char LightHardening_e[436]; 

int annotation_LightHardening_s[] = {HIGH};
int annotation_LightHardening_pk[] = {HIGH};
int annotation_LightHardening_e[] = {LOW};

void LightHardeningEntry(){
    PQCLEAN_MCELIECE348864_CLEAN_encrypt(LightHardening_s, LightHardening_pk, LightHardening_e);
}