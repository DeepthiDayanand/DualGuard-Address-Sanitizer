#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <benchmark/benchmark.h>


extern "C" int crypto_core(unsigned char *out, const unsigned char *in,
                        const unsigned char *k, const unsigned char *c);

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void salsa20_test(benchmark::State& state) {
    uint8_t out[64];
    uint8_t in[16];
    uint8_t key[32]; 
    uint8_t c[16]; 

    generate_random_data(in, 16);
    generate_random_data(out, 64);
    generate_random_data(key, 32);
    generate_random_data(c, 32);
    for ([[maybe_unused]] auto _ : state) {
        crypto_core(out, in, key, c);
    }
}


BENCHMARK(salsa20_test);
BENCHMARK_MAIN();
