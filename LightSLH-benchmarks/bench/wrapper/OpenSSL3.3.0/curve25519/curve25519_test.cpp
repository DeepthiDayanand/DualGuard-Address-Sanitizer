#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <benchmark/benchmark.h>

#ifndef SIZE
#define SIZE (8)
#endif


extern "C" int ossl_x25519(uint8_t out_shared_key[32], const uint8_t private_key[32], const uint8_t peer_public_value[32]);


void generate_random_data(uint8_t *data, size_t size) {
    if (!RAND_bytes(data, size)) {
        fprintf(stderr, "RAND_bytes failed\n");
        exit(1);
    }
}

void curve25519_test(benchmark::State& state) {
    size_t key_len = 32; 
    uint8_t priv_key[key_len], pub_key[key_len], peer_pub_key[key_len], shared_secret[key_len];

    generate_random_data(priv_key, key_len);
    generate_random_data(peer_pub_key, key_len);
    for ([[maybe_unused]] auto _ : state) {
        ossl_x25519(shared_secret, priv_key, peer_pub_key); 
    }
}

BENCHMARK(curve25519_test);
BENCHMARK_MAIN();
