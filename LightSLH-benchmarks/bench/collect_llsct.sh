mkdir llsct && cd llsct

git clone https://github.com/nmosier/clouxx-llvm --depth=1 llsct-llvm
mkdir llsct-llvm/build && cd llsct-llvm/build
cmake -G Ninja -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install -DLLVM_ENABLE_ASSERTIONS=On -DLLVM_ENABLE_PROJECTS='clang;lld' -DLLVM_TARGETS_TO_BUILD='X86' ../llvm
ninja $1
ninja install

cd ../..
git clone https://github.com/nmosier/clouxx-passes llsct-passes
mkdir llsct-passes/build && cd llsct-passes/build
cmake -G Ninja -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLSCT_LLVM_DIR=$PWD/../../llsct-llvm/install -DLLSCT_REQUIRE_CET=Off ..
ninja src/all
