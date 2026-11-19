set -e

cd bench
bash collect_llvm.sh
bash collect_benchmark_lib.sh
bash collect_source_code.sh

cd ..
mkdir -p build
cd build
cmake -G Ninja ..
ninja -j4