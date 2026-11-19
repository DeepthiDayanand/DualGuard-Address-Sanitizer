set -e

# Please set where is your SPEC benchmark installed
SPEC_PATH="../cpu2017"

# We first build the analysis
ROOT_DIR=$(pwd)

# SVF
echo "Building SVF ..."
cd Unified-Memory-Safety-Validation/program-dependence-graph/SVF
bash build.sh

# build llvm-14 from source
cd $ROOT_DIR
echo "Downloading LLVM 14.0.0..."
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/llvm-project-14.0.0.src.tar.xz
echo "Extracting LLVM 14 source code..."
tar -xf llvm-project-14.0.0.src.tar.xz
rm  llvm-project-14.0.0.src.tar.xz

# apply patches
cp LLVM_FIX/SafeStack.cpp llvm-project-14.0.0.src/llvm/lib/CodeGen/SafeStack.cpp
cp LLVM_FIX/X86SpeculativeLoadHardening.cpp llvm-project-14.0.0.src/llvm/lib/Target/X86/X86SpeculativeLoadHardening.cpp

mkdir -p llvm-project-14.0.0.src/build
cd llvm-project-14.0.0.src/build

cmake -G Ninja -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11 -DCMAKE_BUILD_TYPE="Release"  -DLLVM_ENABLE_ASSERTIONS=Off -DLLVM_FORCE_ENABLE_STATS=ON -DLLVM_ENABLE_PROJECTS='clang;lld;compiler-rt' -DLLVM_TARGETS_TO_BUILD='X86' ../llvm -DLLVM_PARALLEL_LINK_JOBS=2
ninja -j10

# PDG
echo "Building PDG ..."
cd $ROOT_DIR/Unified-Memory-Safety-Validation/program-dependence-graph
mkdir -p build
cd build
cmake -G Ninja ..
ninja -j4

# memory analysis
echo "Building Memory Analysis ..."
cd $ROOT_DIR/Unified-Memory-Safety-Validation
mkdir -p build
cd build
cmake -G Ninja ..
ninja -j4

# Baggy
echo "Building Baggy ..."
cd $ROOT_DIR/Unified-Memory-Safety-Validation/BaggyBounds
mkdir -p build
cd build
cmake -G Ninja ..
ninja -j4

# Delta Pointers
echo "Building Delta Pointers ..."
cd $ROOT_DIR/Unified-Memory-Safety-Validation/DeltaPointers
bash ./build.sh


# build LightSLH benchmarks
echo "Building LightSLH benchmarks ..."
cd $ROOT_DIR/LightSLH-benchmarks
bash build.sh

# move run.sh and Makefiles to SPEC folder
echo "Setting up SPEC benchmarks ..."
cd $ROOT_DIR
cp spec-scripts/run.sh $SPEC_PATH
cp spec-scripts/makefiles/* $SPEC_PATH/benchspec

rm $SPEC_PATH/config/*.cfg
cp spec-scripts/config/* $SPEC_PATH/config

if [ ! -d "$SPEC_PATH/scripts" ]
then
    mkdir $SPEC_PATH/scripts
fi
cp spec-scripts/scripts/* $SPEC_PATH/scripts

if [ ! -d "$SPEC_PATH/logs" ]
then
    mkdir $SPEC_PATH/logs
fi

echo "Setup completed."