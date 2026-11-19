#include <benchmark/benchmark.h>
#include <openssl/sha.h>


#ifndef SIZE
#define SIZE (8)
#endif


void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void sha256_test(benchmark::State& state) {
  SHA256_CTX sha256;
  size_t data_len = SIZE; 
  uint8_t input[SIZE];
  generate_random_data(input, data_len);
  SHA256_Init(&sha256);
  for ([[maybe_unused]] auto _ : state) {
    SHA256_Update(&sha256, input, data_len);
  }
}

BENCHMARK(sha256_test);
BENCHMARK_MAIN();
