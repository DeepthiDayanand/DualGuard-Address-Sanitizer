#!/bin/bash
rm ./*.ll

CWD=$(pwd)
workspace="$CWD/../.."
LLVM_PATH="$workspace/bench/llvm-project-14.0.4.src/install/bin"
PQ_PATH="$workspace/bench/crypto_code/PQClean"
CC="$LLVM_PATH/clang -O2 -S -emit-llvm"


mkdir -p "$CWD/../mceliece_ir_temp"
for file in *.c; do
    if [ -f "$file" ]; then
        filename="${file%.*}"
        
        $CC "$file" -o "$filename.ll" "-I/home/zym/llvm-tools/PQClean/common"
        
    fi
done


/usr/zym/llvm/llvm-14.0.4/bin/llvm-link -S  *.ll -o mceliece.ll 

