# enviornment setup
LLVM_DIR=../uslh-ext/llvm-project-14.0.0.src/build/bin
ANALYSIS_DIR=../uslh-ext/Unified-Memory-Safety-Validation

if [ "$#" -eq 2 ]; then
    LLVM_DIR=$1
    ANALYSIS_DIR=$2
fi

PREANALYSIS=".preanalysis"
# PREANALYSIS=""

BENCHMARKS="lbm_r namd_r mcf_r nab_r xz_r deepsjeng_r leela_r x264_r \
            omnetpp_r povray_r perlbench_r parest_r xalancbmk_r"

NUMOFRUNS=4
BENCHMARK_SIZE=train

run_benchmark () {
    NAME=$1-$2
    echo "++++++++Running $NAME++++++++"
    runcpu --config=llvm-$2.cfg --action=build $1 > logs/$NAME.log
    timeout 2h runcpu --config=llvm-$2.cfg --action=run --iterations $NUMOFRUNS --size $BENCHMARK_SIZE $1 > logs/$NAME.log
    python3 scripts/collecting_stats.py $NAME
    rm -f config/*2026*
}

run_baseline() {
    cp benchspec/Makefile.defaults.filter benchspec/Makefile.defaults
    run_benchmark $1 baseline
}

run_analysis() {
    cp benchspec/Makefile.defaults.analysis benchspec/Makefile.defaults
    NAME=$1-analysis
    echo "++++++++Running $NAME++++++++"
    runcpu --config=llvm-analysis.cfg --action=build $1 > logs/$NAME.log
}

run_benchmark_naive_baggy() {
    cp benchspec/Makefile.defaults.naivebaggy$PREANALYSIS benchspec/Makefile.defaults
    for variant in naive-baggy ; do
        run_benchmark $1 $variant
    done
}

run_benchmark_naive_deltapointers() {
    cp benchspec/Makefile.defaults.naivedeltapointer$PREANALYSIS benchspec/Makefile.defaults
    for variant in naive-deltapointers ; do
        run_benchmark $1 $variant
    done
}

# sfi normal ones and slh speculative ones
run_benchmark_hybrid() {
    cp benchspec/Makefile.defaults.hybrid$PREANALYSIS benchspec/Makefile.defaults
    for variant in hybrid-baggy-Aslh hybrid-baggy-aslh hybrid-deltapointers-aslh hybrid-deltapointers-Aslh; do
        run_benchmark $1 $variant
    done
}

run_benchmark_baggy() {
    cp benchspec/Makefile.defaults.baggy$PREANALYSIS benchspec/Makefile.defaults
    for variant in baggy-seq-spec; do
        run_benchmark $1 $variant
    done
}

run_benchmark_deltapointers() {
    cp benchspec/Makefile.defaults.deltapointers$PREANALYSIS benchspec/Makefile.defaults
    for variant in deltapointers-dual ; do
        run_benchmark $1 $variant
    done
}

run_benchmark_naive_safestack() {
    cp benchspec/Makefile.defaults.naivesafestack$PREANALYSIS benchspec/Makefile.defaults
    for variant in naive-safestack ; do
        run_benchmark $1 $variant
    done
}

# safestack only and hybrid are all here
run_benchmark_safestack() {
    cp benchspec/Makefile.defaults.safestack$PREANALYSIS benchspec/Makefile.defaults
    for variant in hybrid-safestack-aslh ; do
        run_benchmark $1 $variant
    done
}

setup_env() {
    ANALYSIS_RESULTS_DIR=$(pwd)/analysis_results

    CXX_BACKEND_LIBCXX=$(g++ -print-file-name=libstdc++.a)
    CC=${LLVM_DIR}/clang
    CXX=${LLVM_DIR}/clang++
    BC_LINKER=${LLVM_DIR}/llvm-link
    BC_OPT=${LLVM_DIR}/opt
    LLVM_DIS=${LLVM_DIR}/llvm-dis
    LLC=${LLVM_DIR}/llc
    C_BACKEND=${LLVM_DIR}/clang
    CXX_BACKEND=${LLVM_DIR}/clang++
    ANALYSIS=${ANALYSIS_DIR}/build/libUnifiedMemSafe.so
    BAGGY=${ANALYSIS_DIR}/BaggyBounds/build/libBaggyBounds.so
    BAGGY_RUNTIME=${ANALYSIS_DIR}/BaggyBounds/build/libbaggy_rt_lib.a

    SVF_CORE=${ANALYSIS_DIR}/program-dependence-graph/SVF/Release-build/svf/libSvfCore.so
    SVF_LLVM=${ANALYSIS_DIR}/program-dependence-graph/SVF/Release-build/svf-llvm/libSvfLLVM.so

    DELTAPOINTER_RT=${ANALYSIS_DIR}/DeltaPointers/build/deltapointers_rt.bc
    DELTAPOINTER_PASS=${ANALYSIS_DIR}/DeltaPointers/build/libdeltapointers.so
    DELTAPOINTER_SHRINK=${ANALYSIS_DIR}/DeltaPointers/build/libshrink_rt.a
    DELTAPOINTER_ADDRSPACE_BITS=32
    DELTAPOINTER_PASS_FLAGS="-instnamer \
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

    for file in ./config/*.cfg; do
        echo "Processing $file"
        sed -i \
            -e "s|^\([[:space:]]*CXX_BACKEND_LIBCXX[[:space:]]*=[[:space:]]*\).*|\1${CXX_BACKEND_LIBCXX}|" \
            -e "s|^\([[:space:]]*CC[[:space:]]*=[[:space:]]*\).*|\1${CC}|" \
            -e "s|^\([[:space:]]*CXX[[:space:]]*=[[:space:]]*\).*|\1${CXX}|" \
            -e "s|^\([[:space:]]*BC_LINKER[[:space:]]*=[[:space:]]*\).*|\1${BC_LINKER}|" \
            -e "s|^\([[:space:]]*BC_OPT[[:space:]]*=[[:space:]]*\).*|\1${BC_OPT}|" \
            -e "s|^\([[:space:]]*LLVM_DIS[[:space:]]*=[[:space:]]*\).*|\1${LLVM_DIS}|" \
            -e "s|^\([[:space:]]*LLC[[:space:]]*=[[:space:]]*\).*|\1${LLC}|" \
            -e "s|^\([[:space:]]*C_BACKEND[[:space:]]*=[[:space:]]*\).*|\1${C_BACKEND}|" \
            -e "s|^\([[:space:]]*CXX_BACKEND[[:space:]]*=[[:space:]]*\).*|\1${CXX_BACKEND}|" \
            -e "s|^\([[:space:]]*ANALYSIS[[:space:]]*=[[:space:]]*\).*|\1${ANALYSIS}|" \
            -e "s|^\([[:space:]]*BAGGY[[:space:]]*=[[:space:]]*\).*|\1${BAGGY}|" \
            -e "s|^\([[:space:]]*BAGGY_RUNTIME[[:space:]]*=[[:space:]]*\).*|\1${BAGGY_RUNTIME}|" \
            -e "s|^\([[:space:]]*SVF_CORE[[:space:]]*=[[:space:]]*\).*|\1${SVF_CORE}|" \
            -e "s|^\([[:space:]]*SVF_LLVM[[:space:]]*=[[:space:]]*\).*|\1${SVF_LLVM}|" \
            -e "s|^\([[:space:]]*DELTAPOINTER_RT[[:space:]]*=[[:space:]]*\).*|\1${DELTAPOINTER_RT}|" \
            -e "s|^\([[:space:]]*DELTAPOINTER_PASS[[:space:]]*=[[:space:]]*\).*|\1${DELTAPOINTER_PASS}|" \
            -e "s|^\([[:space:]]*DELTAPOINTER_PASS_FLAGS[[:space:]]*=[[:space:]]*\).*|\1${DELTAPOINTER_PASS_FLAGS}|" \
            -e "s|^\([[:space:]]*DELTAPOINTER_LINK_FLAGS[[:space:]]*=[[:space:]]*\).*|\1${DELTAPOINTER_LINK_FLAGS}|" \
            -e "s|^\([[:space:]]*ANALYSIS_RESULTS_DIR[[:space:]]*=[[:space:]]*\).*|\1${ANALYSIS_RESULTS_DIR}|" \
            "$file"
    done < <(find ./config -type f -name '*.cfg')
}

setup_env
mkdir -p $ANALYSIS_RESULTS_DIR

# set the corresponding perlbench script
cp scripts/perlbench-$BENCHMARK_SIZE.sh scripts/perlbench.sh

for BENCHMARK in $BENCHMARKS; do
    source shrc
    runcpu --config=llvm-baseline.cfg --action=scrub $BENCHMARK > /dev/null
    run_analysis $BENCHMARK
    run_baseline $BENCHMARK
    run_benchmark_baggy $BENCHMARK
    run_benchmark_deltapointers $BENCHMARK
    run_benchmark_naive_baggy $BENCHMARK
    run_benchmark_naive_deltapointers $BENCHMARK
    run_benchmark_hybrid $BENCHMARK
    run_benchmark_naive_safestack $BENCHMARK
    run_benchmark_safestack $BENCHMARK
done