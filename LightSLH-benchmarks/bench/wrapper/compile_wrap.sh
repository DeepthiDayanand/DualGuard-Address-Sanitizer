#!/bin/bash
set -e

OPT_LEVEL="-O2"
OPT_LEVEL_FILTER="-O2"

current_path=$(pwd) # wrapper
root_dir=$current_path/../../..

OPENSSL3INCLUDE="-I/$current_path/../crypto_code/openssl-3.3.0/include -I/$current_path/../crypto_code/openssl-3.3.0/crypto -I/$current_path/../crypto_code/openssl-3.3.0"

OPENSSL1INCLUDE="-I/$current_path/../crypto_code/openssl-1.0.2f/crypto -I/$current_path/../crypto_code/openssl-1.0.2f/include -I/$current_path/../crypto_code/openssl-1.0.2f"

LIBSODIUM_INLCLUDE="-I/$current_path/../crypto_code/libsodium-1.0.20/src/libsodium/crypto_core/salsa/ref  -I/$current_path/../crypto_code/libsodium-1.0.20/src/libsodium/include/sodium/private  -I/$current_path/../crypto_code/libsodium-1.0.20/src/libsodium/include/sodium"

NACL_POLY_INCLUDE="-I/$current_path/../crypto_code/nacl-20110221/crypto_onetimeauth/poly1305/ref -I/$current_path/NaCL"
NACL_SALSA_INCLUDE="-I/$current_path/../crypto_code/nacl-20110221/crypto_core/salsa20/ref -I/$current_path/NaCL"

CC="$root_dir/llvm-project-14.0.0.src/build/bin/clang -S -g -emit-llvm $OPT_LEVEL -fno-slp-vectorize -fno-vectorize"

FILTER_DIR="$root_dir/llvm-project-14.0.0.src/build/bin"
FILTER_CC="$FILTER_DIR/clang -g -c -emit-llvm $OPT_LEVEL_FILTER -fno-slp-vectorize -fno-vectorize"
SAFESTACK_CC="$FILTER_DIR/clang -g -c -emit-llvm $OPT_LEVEL_FILTER -fno-slp-vectorize -fno-vectorize -fsanitize=safe-stack"

BENCH_INCLUDE="-I/$current_path/../google_benchmark/include"

params=(
    "aes|OpenSSL3.3.0|$OPENSSL3INCLUDE|-DOPENSSL_AES_CONST_TIME"
    "chacha|OpenSSL3.3.0|$OPENSSL3INCLUDE"
    "curve25519|OpenSSL3.3.0|$OPENSSL3INCLUDE"
    "poly1305|OpenSSL3.3.0|$OPENSSL3INCLUDE"
    "sha256|OpenSSL3.3.0|$OPENSSL3INCLUDE"
    "bn|OpenSSL3.3.0|$OPENSSL3INCLUDE"
    "bn|OpenSSL1.0.2f|$OPENSSL1INCLUDE"
    "salsa20|Libsodium1.0.20|$LIBSODIUM_INLCLUDE"
    "sha256|Libsodium1.0.20|$LIBSODIUM_INLCLUDE"
    "salsa20|NaCL|$NACL_SALSA_INCLUDE"
    "sha256|NaCL|$NACL_SHA_INCLUDE"
    "poly1305|NaCL|$NACL_POLY_INCLUDE"
    "kyber|PQClean"
    "mceliece|PQClean"
)

for param in "${params[@]}"; do
    IFS='|' read -r name lib include_path macro <<< "$param"
    # echo "$CC -w $include_path ./$lib/$name/"$name"_wrap.c -o ./$lib/$name/"$name"_wrap.ll $macro" 
    $CC -w $include_path ./$lib/$name/"$name"_wrap.c -o ./$lib/$name/"$name"_wrap.ll $macro -pthread
    $FILTER_CC -w $BENCH_INCLUDE $include_path ./$lib/$name/"$name"_test.cpp -o ./$lib/$name/"$name"_test.bc $macro -pthread
    $FILTER_CC -w $include_path ./$lib/$name/"$name"_wrap.c -o ./$lib/$name/"$name"_wrap_filter.bc $macro -pthread
    $SAFESTACK_CC -w $include_path ./$lib/$name/"$name"_wrap.c -o ./$lib/$name/"$name"_wrap_sfi.bc $macro -pthread
    if [ $name == "bn" ]; then
        if [ "$lib" == "OpenSSL3.3.0" ]; then
            bash collectbn.sh 3.3.0 $OPT_LEVEL $OPT_LEVEL_FILTER
        else
            bash collectbn.sh 1.0.2f $OPT_LEVEL $OPT_LEVEL_FILTER
        fi
    fi
    if [ $name == "kyber" ]; then
        bash collect_kyber.sh $OPT_LEVEL $OPT_LEVEL_FILTER
    fi
    if [ $name == "mceliece" ]; then
        bash collect_mceliece.sh $OPT_LEVEL $OPT_LEVEL_FILTER
    fi
done
