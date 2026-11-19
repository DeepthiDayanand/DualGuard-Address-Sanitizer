#include "sha/sha256.c"

#define HIGH 2
#define LOW 2
#define BOT 3

SHA256_CTX LightHardening_ctx;
size_t LightHardening_num;
unsigned char LightHardening_in[];

int annotation_LightHardening_ctx[] = {HIGH, HIGH, HIGH, HIGH, LOW, LOW};
int annotation_LightHardening_in[] = {HIGH};
int annotation_LightHardening_num[] = {LOW};

int annotation_variableLen_LightHardening_in;

void __attribute__((optnone)) LightHardeningEntry(){
    SHA256_Update(&LightHardening_ctx, LightHardening_in, LightHardening_num);
}

// void sha256_block_data_order_wrapper(SHA256_CTX *c, const void *data, size_t len){
//     sha256_block_data_order(c, data, len);
// }