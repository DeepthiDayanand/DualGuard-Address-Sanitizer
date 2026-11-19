#!/bin/bash
set -e
BENCH_PATH=$(pwd)/..
CRYPTO_PATH="$BENCH_PATH/crypto_code"
LLVM_PATH="$BENCH_PATH/../../llvm-project-14.0.0.src/build/bin"
OPENSSL_INCLUDE="-I/$CRYPTO_PATH/openssl-$1/include -I/$CRYPTO_PATH/openssl-$1/crypto -I/$CRYPTO_PATH/openssl-$1"
OPENSSL_SRC_PATH="$CRYPTO_PATH/openssl-$1/crypto/bn"
CC="$LLVM_PATH/clang"
LINKER="$LLVM_PATH/llvm-link"

OUTPUT_DIR="./llvm_ir_temp"
if [ -d "$OUTPUT_DIR" ]; then
    rm -r $OUTPUT_DIR
fi
mkdir -p $OUTPUT_DIR

# Clang compiler options
CLANG_OPTIONS=" $OPENSSL_INCLUDE -fno-slp-vectorize -fno-vectorize -emit-llvm -g -S $2 "
CLANG_OPTIONS_FILTER=" $OPENSSL_INCLUDE -g -fno-slp-vectorize -fno-vectorize -emit-llvm -c $3 "
CLANG_OPTIONS_SAFESTACK=" $OPENSSL_INCLUDE -g -fno-slp-vectorize -fno-vectorize -emit-llvm -c $3 -fsanitize=safe-stack "

for c_file in $OPENSSL_SRC_PATH/*.c; do
    if [ "$c_file" == "$OPENSSL_SRC_PATH/bn_ppc.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/bn_sparc.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/bn_ctx.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/exp.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/bntest.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/divtest.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/bnspeed.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/exptest.c" ]; then
        continue
    fi
    if [ "$c_file" == "$OPENSSL_SRC_PATH/expspeed.c" ]; then
        continue
    fi

    base_name=$(basename "$c_file" .c)
    $CC $CLANG_OPTIONS  $c_file -o $OUTPUT_DIR/$base_name.ll
    $CC $CLANG_OPTIONS_FILTER $c_file -o $OUTPUT_DIR/"$base_name"_orig.bc
    $CC $CLANG_OPTIONS_SAFESTACK $c_file -o $OUTPUT_DIR/"$base_name"_sfi.bc
done

# # add mem.c into analysis
# $CC $CLANG_OPTIONS $OPENSSL_SRC_PATH/../mem.c -o $OUTPUT_DIR/mem.ll
# $CC $CLANG_OPTIONS_FILTER $OPENSSL_SRC_PATH/../mem.c -o $OUTPUT_DIR/mem_orig.bc
# $CC $CLANG_OPTIONS_SAFESTACK $OPENSSL_SRC_PATH/../mem.c -o $OUTPUT_DIR/mem_sfi.bc

if [ "$1" == "3.3.0" ]; then
    $CC $CLANG_OPTIONS $OPENSSL_SRC_PATH/../ctype.c -o $OUTPUT_DIR/ctype.ll
    $CC $CLANG_OPTIONS_FILTER $OPENSSL_SRC_PATH/../ctype.c -o $OUTPUT_DIR/ctype_orig.bc
    $CC $CLANG_OPTIONS_SAFESTACK $OPENSSL_SRC_PATH/../ctype.c -o $OUTPUT_DIR/ctype_sfi.bc
fi
$LINKER -S $OUTPUT_DIR/*.ll -o $OUTPUT_DIR/linked.ll
$LINKER -S $OUTPUT_DIR/linked.ll ./OpenSSL$1/bn/bn_wrap.ll -o ./OpenSSL$1/bn/bn_wrap.ll

$LINKER $OUTPUT_DIR/*_orig.bc -o $OUTPUT_DIR/linked.bc
$LINKER $OUTPUT_DIR/linked.bc ./OpenSSL$1/bn/bn_wrap_filter.bc -o ./OpenSSL$1/bn/bn_wrap_filter.bc

$LINKER $OUTPUT_DIR/*_sfi.bc -o $OUTPUT_DIR/linked_sfi.bc
$LINKER $OUTPUT_DIR/linked_sfi.bc ./OpenSSL$1/bn/bn_wrap_sfi.bc -o ./OpenSSL$1/bn/bn_wrap_sfi.bc

rm -r $OUTPUT_DIR