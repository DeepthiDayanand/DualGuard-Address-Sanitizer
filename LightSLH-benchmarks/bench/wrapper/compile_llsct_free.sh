#!/bin/bash
set -ex

# $1 dirname
# $2 size
# $3 libname
# $4 benchname
current_path=$(pwd)  # should be in wrapper directory
root_dir=$current_path/../../..

bench_workspace="$current_path/.."
workspace="$bench_workspace/.."
google_benchmark_path="$bench_workspace/google_benchmark"

benchmark_size="$2"
LIBRARY="$3"

LibVAR="$current_path/$3"
BenchVAR="$bench_workspace/google_benchmark/build/src/libbenchmark.a"
IncludeVAR="-I/$google_benchmark_path/include"
VAR="$current_path/$3/$1"

ClangPath="$root_dir/llvm-project-14.0.0.src/build/bin"
# ClangPath="/usr/zym/llvm/llvm-14.0.4/bin"
LLC="$ClangPath/llc"
CC="$ClangPath/clang"
CXX="$ClangPath/clang++"
OPT="$ClangPath/opt"

ANALYSIS_DIR="$root_dir/Unified-Memory-Safety-Validation"

LLVM_DIS="$ClangPath/llvm-dis"

LINKER="$ClangPath/llvm-link"

SVF_CORE="$ANALYSIS_DIR/program-dependence-graph/SVF/Release-build/svf/libSvfCore.so"
SVF_LLVM="$ANALYSIS_DIR/program-dependence-graph/SVF/Release-build/svf-llvm/libSvfLLVM.so"
MEMORY_ANALYSIS="$ANALYSIS_DIR/build/libUnifiedMemSafe.so"
BAGGY="$ANALYSIS_DIR/BaggyBounds/build/libBaggyBounds.so"
BAGGY_RT="$ANALYSIS_DIR/BaggyBounds/build/libbaggy_rt_lib.a"

DELTAPOINTER_RT="$ANALYSIS_DIR/DeltaPointers/build/deltapointers_rt.bc"
DELTAPOINTER_PASS="$ANALYSIS_DIR/DeltaPointers/build/libdeltapointers.so"
DELTAPOINTER_SHRINK="$ANALYSIS_DIR/DeltaPointers/build/libshrink_rt.a"
DELTAPOINTER_ADDRSPACE_BITS=32
DELTAPOINTER_PASS_FLAGS="-instnamer \
        -defer-global-init \
        -expand-const-global-users \
        -replace-address-taken-malloc \
        -loop-simplify \
        -safe-allocs \
        -find-reinterpreted-pointers \
        -deltatags-alloc \
        -deltatags-nullptr=false \
        -address-space-bits="$DELTAPOINTER_ADDRSPACE_BITS" \
        -deltatags-prop \
        -deltatags-check-overflow=none \
        -mask-pointers \
        -mask-pointers-ignore-list=strtok \
        -simplifycfg \
        -custominline \
        -overflow-bit"

DELTAPOINTER_EXPOSED_FUNCTIONS="strsize_nullsafe strtok strtok_ubound rts_gep \
                       rts_load rts_store check_neg_arith mask_pointer_bzhi \
                       mask_pointer_pext_reg mask_pointer_pext_glob execv_mask \
                       execvp_mask execvpe_mask execve_mask writev_mask \
                       is_oob _tag_pointer _mask_pointer _tag_of _take_tag \
                       _ptr_arith"

DELTAPOINTER_LINK_FLAGS="-Wl,--whole-archive $DELTAPOINTER_SHRINK -Wl,--no-whole-archive -ldl"

for fn in $DELTAPOINTER_EXPOSED_FUNCTIONS; do
    DELTAPOINTER_LINK_FLAGS+=" -u__noinstrument_$fn"
done

if [ $3 == 'OpenSSL3.3.0' ]; then
    IncludeVAR="$IncludeVAR -I$bench_workspace/crypto_code/openssl-3.3.0/include"
elif [ $3 == 'OpenSSL1.0.2f' ]; then
    IncludeVAR="$IncludeVAR -I$bench_workspace/crypto_code/openssl-1.0.2f/include"
fi

for opt in LightSLH LightFence fence SSLH  LightCut; do
    rm -f $VAR/${1}_$opt.ll
done 
rm -f $VAR/${1}_test_*
rm -f $VAR/${1}_*.s
rm -f $VAR/${1}_*.o
# rm -f $VAR/${1}_*.txt

$LINKER "$VAR/$1_wrap_filter.bc" -o "$VAR/$1_DG.bc"
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG.bc" -instnamer "$VAR/$1_DG.bc"

# Link delta pointer runtime
$LINKER "$VAR/$1_wrap_filter.bc" "$DELTAPOINTER_RT" -o "$VAR/$1_DP.bc"
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP.bc" -instnamer "$VAR/$1_DP.bc"

$LINKER "$VAR/$1_wrap_sfi.bc" -o "$VAR/$1_sfi.bc"
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_sfi.bc" -instnamer "$VAR/$1_sfi.bc"

# ++++++++++++++++++++++++++++++  OPT (Analysis first, then transformation) ++++++++++++++++++++++++++++++++

# LightSLH
$OPT -opaque-pointers=0 -enable-new-pm=0 -S  -load-pass-plugin=$workspace/build/libSpectrePass.so -passes=lightslh "$VAR/$1_wrap.ll" -o "$VAR/$1_LightSLH.ll" > "$VAR/$1_log_lightslh.txt"  2>&1

# Do all the analysis first, the analysis. The analysis has 4 options: pc, oow, sfi-only, and diff-only, sfi variants has different input bc

# pc off, oow on, sfi-only off, diff-only off (used by FilterSLH)
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG0100.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -unified -entry=LightHardeningEntry < "$VAR/$1_DG.bc" > "$VAR/$1_DG0100.log" 2>&1

# pc on, oow off, sfi-only on, diff-only off (used by baggy-Aslh (1st stage), naive-baggy(1st stage), DP-Aslh (1st stage), naive-DP(1st stage))
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG1010.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=true -enable-oow=false -sfi-only < "$VAR/$1_DG.bc" > "$VAR/$1_DG1010.log" 2>&1

# pc off, oow off, sfi-only on, diff-only off (used by baggy-seq-spec, DP-seq, DP-dual)
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG0010.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=false -enable-oow=false -sfi-only < "$VAR/$1_DG.bc" > "$VAR/$1_DG0010.log" 2>&1

# pc on, oow off, sfi-only off, diff-only off (used by baggy-aslh(1st stage), DP-aslh(1st stage))
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG1000.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=true -enable-oow=false < "$VAR/$1_DG.bc" > "$VAR/$1_DG1000.log" 2>&1

# pc off, oow on, sfi-only off, diff-only on (used by baggy-aslh(2nd stage), DP-aslh(2nd stage))
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG0101.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=false -enable-oow=true -diff-only < "$VAR/$1_DG1000.bc" >> "$VAR/$1_DG0101.log" 2>&1

$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG0100-sfi.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -unified -entry=LightHardeningEntry < "$VAR/$1_DG1010.bc" > "$VAR/$1_DG0100-sfi.log" 2>&1

# Now do the transformations, and the 2nd stages if needed.

# -------- The SLH variants -----------
# FilterSLH
$OPT -opaque-pointers=0 -enable-new-pm=0 -load-pass-plugin=$workspace/build/libSpectrePass.so -passes=filterslh -o "$VAR/$1_FilterSLH_IR.bc" "$VAR/$1_DG0100.bc" > "$VAR/$1_log_filterslh.txt" 2>&1

# -------- The baggy variants ----------
# original baggy
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_BaggyBranch.bc" -load $BAGGY -baggy-bounds -baggy-save-local -baggy-pointers -branchless=false < "$VAR/$1_DG.bc" > "$VAR/$1_BaggyBranch.log" 2>&1

# stacking baggy with SLH, no analysis needed, so should be fast
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_stacking.bc" -load $BAGGY -baggy-bounds -baggy-save-local -baggy-pointers -branchless=false < "$VAR/$1_DG.bc" > "$VAR/$1_stacking.log" 2>&1

# naivebaggy
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_naive-baggy.bc" -load $BAGGY -baggy-bounds -baggy-save-local -baggy-pointers -baggy-dg-safe=true -branchless=false < "$VAR/$1_DG1010.bc" > "$VAR/$1_naive-baggy.log" 2>&1
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_naive-baggy.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=false -enable-oow=true < "$VAR/$1_naive-baggy.bc" >> "$VAR/$1_naive-baggy.log" 2>&1

# baggy-Aslh
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_baggy-ASLH.bc" -load $BAGGY -baggy-bounds -baggy-save-local -baggy-pointers -baggy-dg-safe=true < "$VAR/$1_DG1010.bc" > "$VAR/$1_baggy-ASLH.log" 2>&1
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_baggy-ASLH.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=false -enable-oow=true < "$VAR/$1_baggy-ASLH.bc" >> "$VAR/$1_baggy-ASLH.log" 2>&1

# baggy-aslh
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_baggy-aSLH.bc" -load $BAGGY -baggy-bounds -baggy-save-local -baggy-pointers -baggy-dg-safe=true < "$VAR/$1_DG0101.bc" > "$VAR/$1_baggy-aSLH.log" 2>&1

# dualbaggy
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_baggy-seq-spec.bc" -load $BAGGY -baggy-bounds -baggy-save-local -baggy-pointers -baggy-dg-safe=true < "$VAR/$1_DG0010.bc" > "$VAR/$1_baggy-seq-spec.log" 2>&1


# -------- The delta pointer variants ----------
# link the runtime first
$LINKER "$VAR/$1_DG1010.bc" "$DELTAPOINTER_RT" -o "$VAR/$1_DG1010-DP.bc"
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG1010-DP.bc" -instnamer "$VAR/$1_DG1010-DP.bc"

$LINKER "$VAR/$1_DG0101.bc" "$DELTAPOINTER_RT" -o "$VAR/$1_DG0101-DP.bc"
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG0101-DP.bc" -instnamer "$VAR/$1_DG0101-DP.bc"

$LINKER "$VAR/$1_DG0010.bc" "$DELTAPOINTER_RT" -o "$VAR/$1_DG0010-DP.bc"
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DG0010-DP.bc" -instnamer "$VAR/$1_DG0010-DP.bc"

# original DP
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP-original.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -stats < "$VAR/$1_DP.bc" > "$VAR/$1_DP-original.log" 2>&1

# stacking DP with SLH, no analysis needed, so should be fast
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_stackingDP.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -stats < "$VAR/$1_DP.bc" > "$VAR/$1_stackingDP.log" 2>&1

# naiveDP
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_naive-DP.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -dp-dg-safe -stats < "$VAR/$1_DG1010-DP.bc" > "$VAR/$1_naive-DP.log" 2>&1
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_naive-DP.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=false -enable-oow=true < "$VAR/$1_naive-DP.bc" >> "$VAR/$1_naive-DP.log" 2>&1

# DP-Aslh
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP-ASLH.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -dp-dg-safe -spec-safe-masking -stats < "$VAR/$1_DG1010-DP.bc" > "$VAR/$1_DP-ASLH.log" 2>&1
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP-ASLH.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -entry=LightHardeningEntry -unified -use-pc=false -enable-oow=true < "$VAR/$1_DP-ASLH.bc" >> "$VAR/$1_DP-ASLH.log" 2>&1

# DP-aslh
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP-aSLH.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -dp-dg-safe -spec-safe-masking -stats < "$VAR/$1_DG0101-DP.bc" > "$VAR/$1_DP-aSLH.log" 2>&1

# dualDP
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP-dual.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -dp-dg-safe -spec-safe-masking -stats < "$VAR/$1_DG0010-DP.bc" > "$VAR/$1_DP-dual.log" 2>&1
# DP-seq
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_DP-seq.bc" -load $DELTAPOINTER_PASS $DELTAPOINTER_PASS_FLAGS -dp-dg-safe -stats < "$VAR/$1_DG0010-DP.bc" > "$VAR/$1_DP-seq.log" 2>&1


# -------- The safestack variants ----------
# safestack stacking with SLH
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_stacking_sfi.bc" < "$VAR/$1_sfi.bc" > "$VAR/$1_stacking_sfi.log" 2>&1

# naive safestack
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_naive-sfi.bc" < "$VAR/$1_DG0100-sfi.bc" > "$VAR/$1_naive-sfi.log" 2>&1

# safestack + selective SLH
$OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_sfi-aSLH.bc" < "$VAR/$1_DG0101.bc" > "$VAR/$1_sfi-aSLH.log" 2>&1

# add safestack attribute
for opt in naive-sfi sfi-aSLH; do
    $OPT -opaque-pointers=0 -enable-new-pm=0 -o "$VAR/$1_$opt.bc" -load $SVF_CORE -load $SVF_LLVM -load $MEMORY_ANALYSIS -ir-safestack < "$VAR/$1_$opt.bc" >> "$VAR/$1_$opt.txt" 2>&1
done


# ++++++++++++++++++++++++++++++  LLC ++++++++++++++++++++++++++++++++

$LLC $VAR/$1_wrap.ll -o $VAR/$1_origin.s

# LLVM-SLH
$LLC -x86-speculative-load-hardening -x86-slh-post-load="false" -x86-slh-store="true" -stats "$VAR/$1_FilterSLH_IR.bc" -o "$VAR/$1_ASLH.s" > "$VAR/$1_ASLH.log" 2>&1

# SLH at assembly level
$LLC -x86-speculative-load-hardening -x86-slh-post-load="false" -x86-slh-store="true" -x86-slh-filter -x86-slh-filter-store -stats "$VAR/$1_FilterSLH_IR.bc" -o "$VAR/$1_aSLH.s" >> "$VAR/$1_aSLH.log" 2>&1

# LightSLH
for opt in LightSLH; do
   $LLC $VAR/$1_$opt.ll -o $VAR/$1_$opt.s
done 

# now we do the variants don't need SLH
for opt in FilterSLH_IR BaggyBranch baggy-seq-spec DP-original DP-seq DP-dual; do
   $LLC $VAR/$1_$opt.bc -o $VAR/$1_$opt.s
done 

# then we do the variants need SLH but no filtering
for opt in stacking stackingDP stacking_sfi; do
    $LLC -x86-speculative-load-hardening -x86-slh-post-load="false" -x86-slh-store="true" -stats  $VAR/$1_$opt.bc -o $VAR/$1_$opt.s >> "$VAR/$1_$opt.log" 2>&1
done

# then we do the variants need SLH and filtering
for opt in naive-baggy baggy-ASLH baggy-aSLH naive-DP DP-ASLH DP-aSLH; do
    $LLC -x86-speculative-load-hardening -x86-slh-post-load="false" -x86-slh-store="true" -x86-slh-filter -x86-slh-filter-store -stats  $VAR/$1_$opt.bc -o $VAR/$1_$opt.s >> "$VAR/$1_$opt.log" 2>&1
done 

# safestack + SLH
for opt in sfi-aSLH naive-sfi; do
    $LLC -x86-speculative-load-hardening -x86-slh-post-load="false" -x86-slh-store="true" -x86-slh-filter -x86-slh-filter-store -stats -dg-safe=true  $VAR/$1_$opt.bc -o $VAR/$1_$opt.s >> "$VAR/$1_$opt.log" 2>&1
done 



# ++++++++++++++++++++++++++++++  Backend ++++++++++++++++++++++++++++++++

link_to_executable() {
    $CC -c  "$VAR/$1_$2.s" -o "$VAR/$1_$2.o" -Wno-deprecated-declarations -fPIE -pthread
    if [[ "$LIBRARY" == OpenSSL* ]]; then
        $CXX  -D SIZE=$benchmark_size -o "$VAR/$1_test_$2"  "$VAR/$1_$2.o" $3 $IncludeVAR $BenchVAR  $LibVAR/libssl.a $LibVAR/libcrypto.a -Wl,--allow-multiple-definition -Wno-deprecated-declarations -pthread -ldl
    elif [[ "$LIBRARY" == Libsodium* ]]; then
        $CXX -fPIE -DSIZE=$benchmark_size -o "$VAR/$1_test_$2" "$VAR/$1_$2.o" $3 $IncludeVAR $BenchVAR $LibVAR/libsodium.a -Wl,--allow-multiple-definition -Wno-deprecated-declarations -pthread
    else
        $CXX  -D SIZE=$benchmark_size -o "$VAR/$1_test_$2"  "$VAR/$1_$2.o" $3 $IncludeVAR $BenchVAR -Wl,--allow-multiple-definition -Wno-deprecated-declarations -pthread
    fi
}

for opt in origin LightSLH FilterSLH_IR ASLH aSLH ; do
    link_to_executable $1 $opt "$VAR/$1_test.cpp"
done 

for opt in BaggyBranch stacking naive-baggy baggy-ASLH baggy-aSLH baggy-seq-spec ; do
    link_to_executable $1 $opt "$VAR/$1_test.cpp $BAGGY_RT "
done

# safe stack need additional flags
for opt in stacking_sfi sfi-aSLH naive-sfi; do
    link_to_executable $1 $opt "-fsanitize=safe-stack $VAR/$1_test.cpp"
done

# Delta pointers need additional flags
for opt in DP-original stackingDP naive-DP DP-ASLH DP-aSLH DP-seq DP-dual ; do
    link_to_executable $1 $opt "$VAR/$1_test.cpp $DELTAPOINTER_LINK_FLAGS"
done