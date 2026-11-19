#!/bin/bash
set -e

# $1 dirname
# $2 size
# $3 libname
# $4 benchname
current_path=$(pwd)  # should be in wrapper directory
bench_workspace="$current_path/.."
workspace="$bench_workspace/.."
google_benchmark_path="$bench_workspace/google_benchmark"
llsct_path="$bench_workspace/llsct"

LibVAR="$current_path/$3"
BenchVAR="$bench_workspace/google_benchmark/build/src/libbenchmark.a"
IncludeVAR="-I/$google_benchmark_path/include"
VAR="$current_path/$3/$1"

ClangPath="$bench_workspace/llvm-project-14.0.4.src/install/bin"
# ClangPath="/usr/zym/llvm/llvm-14.0.4/bin"
LLC="$ClangPath/llc"
CC="$ClangPath/clang"
CXX="$ClangPath/clang++"
OPT="$ClangPath/opt"

if [ $3 == 'OpenSSL3.3.0' ]; then
    IncludeVAR="$IncludeVAR -I$bench_workspace/crypto_code/openssl-3.3.0/include"
elif [ $3 == 'OpenSSL1.0.2f' ]; then
    IncludeVAR="$IncludeVAR -I$bench_workspace/crypto_code/openssl-1.0.2f/include"
fi

LLSCT_VAR="$llsct_path/llsct-llvm/install/bin/clang  -fPIC -m64  -fno-stack-protector -fcf-protection=none -fno-jump-tables -mno-red-zone -fcf-protection=full -flegacy-pass-manager -Xclang -load -Xclang $llsct_path/llsct-passes/build/src/libDuplicatePass.so -Xclang -load -Xclang $llsct_path/llsct-passes/build/src/libMemIntrinsicPass.so -Xclang -load -Xclang $llsct_path/llsct-passes/build/src/libInlinePass.so -Xclang -load -Xclang $llsct_path/llsct-passes/build/src/libAttributes.so -Xclang -load -Xclang $llsct_path/llsct-passes/build/src/libMitigatePass.so -Xclang -load -Xclang $llsct_path/llsct-passes/build/src/libFunctionLocalStacks.so -mllvm -no-stack-slot-sharing -mllvm -no-promote-arguments -mllvm -clou=+ncal_xmit,ncal_glob,ncas_xmit,ncas_ctrl  -mllvm -clou=+prech -mllvm -clou-log=$llsct_path/llsct-passes/build/bench/openssl_llsct/logs -S -emit-llvm "

for opt in LightSLH LightFence fence SSLH llsct LightCut; do
    rm -f $VAR/${1}_$opt.ll
done 
rm -f $VAR/${1}_test_*
rm -f $VAR/${1}_*.s
rm -f $VAR/${1}_*.o
rm -f $VAR/${1}_*.txt

if [ "$4" != "RSA" ]; then
    $LLSCT_VAR "$VAR/$1_wrap.ll" -o "$VAR/$1_llsct.ll" 
fi


$OPT -S  -load-pass-plugin=$workspace/build/libSpectrePass.so -passes=lightslh "$VAR/$1_wrap.ll" -o "$VAR/$1_LightSLH.ll"

$OPT -S  -load-pass-plugin=$workspace/build/libSpectrePass.so -passes=sslh "$VAR/$1_wrap.ll" -o "$VAR/$1_SSLH.ll" > /dev/null

$OPT -S  -load-pass-plugin=$workspace/build/libSpectrePass.so -passes=lightfence "$VAR/$1_wrap.ll" -o "$VAR/$1_LightFence.ll" > "$VAR/$1_log_fence.txt"

$OPT -S  -load-pass-plugin=$workspace/build/libSpectrePass.so -passes=lightcut "$VAR/$1_wrap.ll" -o "$VAR/$1_LightCut.ll" > "$VAR/$1_log_cut.txt"

$LLC -mtriple=x86_64-unknown-linux-gnu -x86-speculative-load-hardening -x86-slh-lfence  $VAR/$1_wrap.ll -o $VAR/$1_fence.s

$LLC $VAR/$1_wrap.ll -o $VAR/$1_origin.s

for opt in LightSLH LightFence SSLH llsct LightCut; do
    if [[ "$opt" == "llsct" && "$4" == "RSA" ]]; then
        continue
    fi
   $LLC $VAR/$1_$opt.ll -o $VAR/$1_$opt.s
done 

for opt in origin LightSLH LightFence fence SSLH llsct LightCut; do
    if [[ "$opt" == "llsct" && "$4" == "RSA" ]]; then
        continue
    fi
    $CC -c  "$VAR/$1_$opt.s" -o "$VAR/$1_$opt.o" -Wno-deprecated-declarations -fPIE
    if [[ "$3" == OpenSSL* ]]; then
        $CXX  -D SIZE=$2 -o "$VAR/$1_test_$opt" "$VAR/$1_test.cpp" "$VAR/$1_$opt.o" $IncludeVAR $BenchVAR $LibVAR/libssl.a $LibVAR/libcrypto.a -Wl,--allow-multiple-definition -Wno-deprecated-declarations
    elif [[ "$3" == Libsodium* ]]; then
        $CXX -fPIE -DSIZE=$2 -o "$VAR/$1_test_$opt" "$VAR/$1_test.cpp" "$VAR/$1_$opt.o" $IncludeVAR $BenchVAR  $LibVAR/libsodium.a -Wl,--allow-multiple-definition -Wno-deprecated-declarations
    else
        $CXX  -D SIZE=$2 -o "$VAR/$1_test_$opt" "$VAR/$1_test.cpp" "$VAR/$1_$opt.o" $IncludeVAR $BenchVAR  -Wl,--allow-multiple-definition -Wno-deprecated-declarations
    fi
done 
