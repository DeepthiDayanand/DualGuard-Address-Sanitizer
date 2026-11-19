# set -ex

# set your SPEC2017 path
ROOT_DIR=$(pwd)
SPEC_PATH="../cpu2017"

# no argument: run all benchmarks
# 1 argument: spec or lightslh

run_spec() {
    echo "Running SPEC2017 benchmarks ..."
    pushd $SPEC_PATH
    bash ./run.sh $ROOT_DIR/llvm-project-14.0.0.src/build/bin $ROOT_DIR/Unified-Memory-Safety-Validation
    popd
}

run_lightslh() {
    echo "Running LightSLH benchmarks ..."
    pushd $ROOT_DIR/LightSLH-benchmarks/bench/wrapper
    bash compile_wrap.sh
    bash bench_llsct_free.sh compile
    bash bench_llsct_free.sh run
    popd
}


if [ "$1" == "spec" ]; then
    run_spec
elif [ "$1" == "lightslh" ]; then
    run_lightslh
else
    run_lightslh
    run_spec
fi