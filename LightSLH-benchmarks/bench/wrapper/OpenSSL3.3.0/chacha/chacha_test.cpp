#include <benchmark/benchmark.h>

#ifndef SIZE
#define SIZE (8)
#endif

extern "C" void  ChaCha20_ctr32(uint8_t *out, const uint8_t *in, size_t inlen, const uint32_t key[8], const uint32_t counter[4]);

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void chacha_test(benchmark::State& state) {
    uint8_t key[32];
    uint8_t counter[16];
    size_t data_len = SIZE; 
    uint8_t *input = (uint8_t *)malloc(data_len);
    uint8_t *output = (uint8_t *)malloc(data_len);

    generate_random_data(key, sizeof(key));
    generate_random_data(counter, sizeof(counter));
    generate_random_data(input, data_len);

    unsigned long long int start = 0, end = 0, total = -1;
    for ([[maybe_unused]] auto _ : state) {
        ChaCha20_ctr32(output, input, data_len, (const uint32_t *)key, (const uint32_t *)counter);    }
    free(input);
    free(output);
}
BENCHMARK(chacha_test);
BENCHMARK_MAIN();
