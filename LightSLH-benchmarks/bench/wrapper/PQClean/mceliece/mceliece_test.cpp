#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cstring>
#include <benchmark/benchmark.h>


#ifndef SIZE
#define SIZE (8)
#endif

extern "C"{
void PQCLEAN_MCELIECE348864_CLEAN_encrypt(unsigned char *s, const unsigned char *pk, unsigned char *e);
}

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void mceliece_test(benchmark::State& state) {
    unsigned char s[96];  
    unsigned char pk[261120]; 
    unsigned char e[436];  


    memset(pk, 0xAB, sizeof(pk));
    memset(e, 0, sizeof(e)); 
    

    for ([[maybe_unused]] auto _ : state) {
        PQCLEAN_MCELIECE348864_CLEAN_encrypt(s, pk, e);
    }
}


BENCHMARK(mceliece_test);
BENCHMARK_MAIN();