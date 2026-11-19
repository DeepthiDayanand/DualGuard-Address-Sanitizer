#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cstring>
#include <benchmark/benchmark.h>


#ifndef SIZE
#define SIZE (8)
#endif

extern "C"{
int PQCLEAN_KYBER512_CLEAN_crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
}

void generate_public_key(uint8_t *pk) {
    for (int i = 0; i < 1184; i++) {
        pk[i] = rand() % 256; 
    }
}


void kyber_test(benchmark::State& state) {
    uint8_t ct[736];
    uint8_t ss[1632];
    uint8_t pk[800];


    generate_public_key(pk);
    

    for ([[maybe_unused]] auto _ : state) {
        PQCLEAN_KYBER512_CLEAN_crypto_kem_enc(ct, ss, pk);
    }
}


BENCHMARK(kyber_test);
BENCHMARK_MAIN();