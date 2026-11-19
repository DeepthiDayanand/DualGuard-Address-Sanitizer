#define NO_FUNCTIONS
#include "openssl/bn.h"
#include "bn/bn_ctx.c"
// #include <openssl/bn.h>
#undef NO_FUNCTIONS





#define HIGH 2
#define LOW 2
#define BOT 3

BIGNUM LightHardening_rr;
BIGNUM LightHardening_a;
BIGNUM LightHardening_p;
BIGNUM LightHardening_m;
BN_ULONG LightHardening_d[];
unsigned int LightHardening_index[];

BN_CTX LightHardening_ctx;
BN_POOL_ITEM LightHardening_bn_pool_item;

BN_MONT_CTX LightHardening_in_mont;

int annotation_LightHardening_rr[] = {BOT, HIGH, HIGH, HIGH, LOW};
int annotation_LightHardening_a[] = {BOT, HIGH, HIGH, HIGH, LOW};
int annotation_LightHardening_p[] = {BOT, HIGH, HIGH, HIGH, LOW};
int annotation_LightHardening_m[] = {BOT, HIGH, HIGH, HIGH, LOW};
int annotation_LightHardening_d[] = {HIGH};
int annotation_LightHardening_index[] = {LOW};

int annotation_LightHardening_in_mont[] = {LOW,
                                           BOT, HIGH, HIGH, HIGH, LOW,
                                           BOT, HIGH, HIGH, HIGH, LOW,
                                           BOT, HIGH, HIGH, HIGH, LOW,
                                           HIGH, LOW};

int annotation_LightHardening_ctx[] = {BOT, BOT, BOT, LOW, LOW, // BN_POOL
                                       BOT, LOW, LOW,           // bn_stack
                                       LOW, LOW, LOW};
int annotation_LightHardening_bn_pool_item[] = {BOT, HIGH, HIGH, HIGH, LOW, // BIGNUM
                                              BOT, BOT};

int annotation_variableLen_LightHardening_d;
int annotation_variableLen_LightHardening_index;

int annotation_skip_function_BN_mod_exp_mont;

void __attribute__((optnone)) LightHardeningEntry(){
    LightHardening_rr.d = LightHardening_d;
    LightHardening_a.d = LightHardening_d;
    LightHardening_p.d = LightHardening_d;
    LightHardening_m.d = LightHardening_d;

    LightHardening_ctx.pool.head = &LightHardening_bn_pool_item;
    LightHardening_ctx.pool.current = &LightHardening_bn_pool_item;
    LightHardening_ctx.pool.tail = &LightHardening_bn_pool_item;
    LightHardening_ctx.stack.indexes = LightHardening_index;

    LightHardening_bn_pool_item.vals[0].d = LightHardening_d;
    LightHardening_bn_pool_item.prev = &LightHardening_bn_pool_item;
    LightHardening_bn_pool_item.next = &LightHardening_bn_pool_item;

    LightHardening_in_mont.RR.d = LightHardening_d;
    LightHardening_in_mont.N.d = LightHardening_d;
    LightHardening_in_mont.Ni.d = LightHardening_d;


    BN_mod_exp_mont_consttime(&LightHardening_rr, &LightHardening_a, &LightHardening_p,
                          &LightHardening_m, &LightHardening_ctx, &LightHardening_in_mont);
}