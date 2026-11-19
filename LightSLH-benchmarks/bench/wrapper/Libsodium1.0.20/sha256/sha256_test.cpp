#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <benchmark/benchmark.h>

#ifndef SIZE
#define SIZE (64)
#endif

typedef struct crypto_hash_sha256_state {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[64];
} crypto_hash_sha256_state;

extern "C" int crypto_hash_sha256_update(crypto_hash_sha256_state *state,
                              const unsigned char *in, unsigned long long inlen);

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void sha256_test(benchmark::State& state) {

    crypto_hash_sha256_state shastate;
    size_t data_len = SIZE; 
    uint8_t *in = (uint8_t *)malloc(data_len);
    generate_random_data(in, data_len);

    for ([[maybe_unused]] auto _ : state) {
        crypto_hash_sha256_update(&shastate, in, data_len);
    }

}
BENCHMARK(sha256_test);
BENCHMARK_MAIN();
