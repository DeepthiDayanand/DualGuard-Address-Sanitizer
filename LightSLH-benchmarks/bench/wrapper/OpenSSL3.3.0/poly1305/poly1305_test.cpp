#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>
extern "C" {
#include "poly1305.h"
}
#include <benchmark/benchmark.h>


#ifndef SIZE
#define SIZE (8)
#endif

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void poly1305_test(benchmark::State& state) {
    size_t data_len = SIZE; 
    uint8_t *input = (uint8_t *)malloc(data_len);
    uint8_t key[32]; 
    uint8_t mac[16]; 

    generate_random_data(key, sizeof(key));
    generate_random_data(input, data_len);

    POLY1305 ctx;
    Poly1305_Init(&ctx, key);

    for ([[maybe_unused]] auto _ : state) {
        Poly1305_Update(&ctx, input, data_len);
    }

    Poly1305_Final(&ctx, mac);
    free(input);
}

BENCHMARK(poly1305_test);
BENCHMARK_MAIN();
