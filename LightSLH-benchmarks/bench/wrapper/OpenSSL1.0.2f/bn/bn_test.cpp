#include <stdio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <benchmark/benchmark.h>


void bn_test(benchmark::State& state) {
    // Initialize OpenSSL BIGNUM structures
    BIGNUM *a = NULL, *p = NULL, *m = NULL, *result = NULL;
    BN_CTX *ctx = NULL;

    a = BN_new();
    p = BN_new();
    m = BN_new();
    result = BN_new();
    ctx = BN_CTX_new();
    
    BN_dec2bn(&a, "12345678910123456781234567891012345678");       
    BN_dec2bn(&p, "246837491520079321661864550401357775527");     
    BN_dec2bn(&m, "565869183302849972945622606303246904073");      

    for ([[maybe_unused]] auto _ : state) {
        BN_mod_exp_mont_consttime(result, a, p, m, ctx, NULL);
    }
    
    BN_free(a);
    BN_free(p);
    BN_free(m);
    BN_free(result);
    BN_CTX_free(ctx);
}

BENCHMARK(bn_test);
BENCHMARK_MAIN();


