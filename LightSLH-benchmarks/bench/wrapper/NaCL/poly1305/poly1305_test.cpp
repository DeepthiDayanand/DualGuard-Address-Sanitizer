#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <benchmark/benchmark.h>

#ifndef SIZE
#define SIZE (8)
#endif

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

extern "C" int crypto_onetimeauth(unsigned char *out, const unsigned char *in, unsigned long long inlen, const unsigned char *k);

void poly1305_test(benchmark::State& state) {
    size_t data_len = SIZE; 
    uint8_t *input = (uint8_t *)malloc(data_len);
    uint8_t key[32]; 
    uint8_t out[16]; 


    generate_random_data(key, sizeof(key));
    generate_random_data(input, data_len);
    for ([[maybe_unused]] auto _ : state) {
        crypto_onetimeauth(out,input,data_len,key);
    }
    free(input);
}
BENCHMARK(poly1305_test);
BENCHMARK_MAIN();
