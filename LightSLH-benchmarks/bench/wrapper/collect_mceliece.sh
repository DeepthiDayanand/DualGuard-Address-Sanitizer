#!/bin/bash
set -e
current_path=$(pwd)  # should be in wrapper directory
bench_workspace="$current_path/.."
workspace="$bench_workspace/.."

ClangPath="$bench_workspace/llvm-14.0.4.obj/bin"
CLANG="$ClangPath/clang"
CC="$CLANG -S -g -fno-slp-vectorize -fno-vectorize -emit-llvm $1"
llvm_link="$ClangPath/llvm-link"
pq_path="$workspace/bench/crypto_code/PQClean/common"
mce_path="$workspace/bench/crypto_code/PQClean/crypto_kem/mceliece348864/clean"
OUTPUT_DIR="./llvm_ir_temp"

FILTER_CC="$CLANG -g -c -fno-slp-vectorize -fno-vectorize -emit-llvm $2"
SAFESTACK_CC="$CLANG -g -c -emit-llvm $2 -fno-slp-vectorize -fno-vectorize -fsanitize=safe-stack"

if [ -d "$OUTPUT_DIR" ]; then
    rm -r $OUTPUT_DIR
fi
mkdir -p $OUTPUT_DIR

cp $pq_path/aes.c $mce_path/aes.c
cp $pq_path/fips202.c $mce_path/sha2.c
cp $pq_path/randombytes.c $mce_path/randombytes.c

for file in $mce_path/*.c; do
    if [ -f "$file" ]; then
        base_name=$(basename "$file" .c)
        $CC "$file" -o "$OUTPUT_DIR/$base_name.ll" "-I$pq_path"
        $FILTER_CC "$file" -o "$OUTPUT_DIR/"$base_name"_orig.bc" "-I$pq_path"
        $SAFESTACK_CC "$file" -o "$OUTPUT_DIR/"$base_name"_sfi.bc" "-I$pq_path"
    fi
done


$llvm_link -S  $OUTPUT_DIR/*.ll -o $OUTPUT_DIR/mceliece.ll 
$llvm_link -S  $OUTPUT_DIR/mceliece.ll $current_path/PQClean/mceliece/mceliece_wrap.ll -o $current_path/PQClean/mceliece/mceliece_wrap.ll

$llvm_link $OUTPUT_DIR/*_orig.bc -o $OUTPUT_DIR/mceliece.bc
$llvm_link $OUTPUT_DIR/mceliece.bc $current_path/PQClean/mceliece/mceliece_wrap_filter.bc -o $current_path/PQClean/mceliece/mceliece_wrap_filter.bc

$llvm_link $OUTPUT_DIR/*_sfi.bc -o $OUTPUT_DIR/mceliece_sfi.bc
$llvm_link $OUTPUT_DIR/mceliece_sfi.bc $current_path/PQClean/mceliece/mceliece_wrap_sfi.bc -o $current_path/PQClean/mceliece/mceliece_wrap_sfi.bc