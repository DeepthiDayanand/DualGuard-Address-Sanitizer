#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <benchmark/benchmark.h>

#ifndef SIZE
#define SIZE (8192)
#endif

typedef struct crypto_hash_sha256_state {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[64];
} crypto_hash_sha256_state;

extern "C" int crypto_hashblocks(unsigned char *statebytes,
                              const unsigned char *in, unsigned long long inlen);

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void sha256_test(benchmark::State& state) {
    unsigned char shastate[32];
    size_t data_len = SIZE; 
    uint8_t *in = (uint8_t *)malloc(data_len);
    generate_random_data(in, data_len);
    for ([[maybe_unused]] auto _ : state) {
        crypto_hashblocks(shastate, in, data_len);
    }
}
BENCHMARK(sha256_test);
BENCHMARK_MAIN();
