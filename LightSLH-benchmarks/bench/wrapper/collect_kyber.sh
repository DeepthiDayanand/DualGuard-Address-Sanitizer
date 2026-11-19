#!/bin/bash
set -e
current_path=$(pwd)  # should be in wrapper directory
bench_workspace="$current_path/.."
workspace="$bench_workspace/.."

ClangPath="$bench_workspace/../../llvm-project-14.0.0.src/build/bin"
CLANG="$ClangPath/clang"
CC="$CLANG -S -g -emit-llvm -fno-slp-vectorize -fno-vectorize $1"
llvm_link="$ClangPath/llvm-link"
pq_path="$workspace/bench/crypto_code/PQClean/common"
kyber_path="$workspace/bench/crypto_code/PQClean-Kyber/crypto_kem/kyber512/clean"
OUTPUT_DIR="./llvm_ir_temp"

FILTER_CC="$CLANG -g -c -emit-llvm -fno-slp-vectorize -fno-vectorize $2"
SAFESTACK_CC="$CLANG -g -c -emit-llvm $2 -fno-slp-vectorize -fno-vectorize -fsanitize=safe-stack"

if [ -d "$OUTPUT_DIR" ]; then
    rm -r $OUTPUT_DIR
fi
mkdir -p $OUTPUT_DIR

cp $pq_path/aes.c $kyber_path/aes.c
cp $pq_path/fips202.c $kyber_path/sha2.c
cp $pq_path/randombytes.c $kyber_path/randombytes.c

for file in $kyber_path/*.c; do
    if [ -f "$file" ]; then
        base_name=$(basename "$file" .c)
        $CC "$file" -o "$OUTPUT_DIR/$base_name.ll" "-I$pq_path"
        $FILTER_CC "$file" -o "$OUTPUT_DIR/"$base_name"_orig.bc" "-I$pq_path"
        $SAFESTACK_CC "$file" -o "$OUTPUT_DIR/"$base_name"_sfi.bc" "-I$pq_path"
    fi
done


$llvm_link -S  $OUTPUT_DIR/*.ll -o $OUTPUT_DIR/kyber.ll 
$llvm_link -S  $OUTPUT_DIR/kyber.ll $current_path/PQClean/kyber/kyber_wrap.ll -o $current_path/PQClean/kyber/kyber_wrap.ll

$llvm_link $OUTPUT_DIR/*_orig.bc -o $OUTPUT_DIR/kyber.bc
$llvm_link $OUTPUT_DIR/kyber.bc $current_path/PQClean/kyber/kyber_wrap_filter.bc -o $current_path/PQClean/kyber/kyber_wrap_filter.bc

$llvm_link $OUTPUT_DIR/*_sfi.bc -o $OUTPUT_DIR/kyber_sfi.bc
$llvm_link $OUTPUT_DIR/kyber_sfi.bc $current_path/PQClean/kyber/kyber_wrap_sfi.bc -o $current_path/PQClean/kyber/kyber_wrap_sfi.bc