#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <openssl/aes.h>
#include <benchmark/benchmark.h>
#ifndef SIZE
#define SIZE (8)
#endif

void generate_random_data(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

void aes_test(benchmark::State& state) {
    AES_KEY aes_key;
    uint8_t key[16]; 
    uint8_t iv[AES_BLOCK_SIZE]; 
    size_t data_len = 1024*1024; 
    uint8_t *input = (uint8_t*)malloc(data_len);
    uint8_t *output = (uint8_t*)malloc(data_len);


    generate_random_data(key, sizeof(key));
    generate_random_data(iv, sizeof(iv));
    generate_random_data(input, data_len);

    AES_set_encrypt_key(key, 128, &aes_key);
    for ([[maybe_unused]] auto _ : state) {
        AES_encrypt(input, output, &aes_key);
        
    }
    free(input);
    free(output);
}


BENCHMARK(aes_test);
BENCHMARK_MAIN();