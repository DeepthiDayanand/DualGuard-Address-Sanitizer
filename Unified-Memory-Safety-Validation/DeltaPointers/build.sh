
ROOT_DIR=$(dirname $(realpath $0))
BUILD_DIR=$ROOT_DIR/build

RT_SRC_DIR=$ROOT_DIR/runtime
RT_BITCODE_DIR=$BUILD_DIR/runtime_bitcode
RT_BITCODE_NAME=deltapointers_rt.bc
RT_BITCODE_FILE=$BUILD_DIR/$RT_BITCODE_NAME

ADDRSPACE_BITS=32

CLANG=$ROOT_DIR/../../llvm-project-14.0.0.src/build/bin/clang
LLVMLINK=$ROOT_DIR/../../llvm-project-14.0.0.src/build/bin/llvm-link
LLVMDIS=$ROOT_DIR/../../llvm-project-14.0.0.src/build/bin/llvm-dis

# make delta pointer pass and libshrink runtime
mkdir -p $BUILD_DIR && cd $BUILD_DIR
cmake -G Ninja ..
ninja -j4

# compile runtime into LLVM bitcode
cd $ROOT_DIR
mkdir -p $RT_BITCODE_DIR

RT_CFLAGS="-emit-llvm -O0 -c -g \
           -DADDRSPACE_BITS="$ADDRSPACE_BITS" \
           -DDELTAPOINTERS \
           -I$RT_SRC_DIR/include"

for src in $RT_SRC_DIR/*.c; do
    src_name=$(basename $src)
    bitcode_name="${src_name%.c}.bc"
    if [[ $src_name == "mask.c" ]]; then
        $CLANG $RT_CFLAGS -mbmi2 -o $RT_BITCODE_DIR/$bitcode_name $src
    else
        $CLANG $RT_CFLAGS -o $RT_BITCODE_DIR/$bitcode_name $src
    fi
    $LLVMDIS $RT_BITCODE_DIR/$bitcode_name -o $RT_BITCODE_DIR/${bitcode_name%.bc}.ll
done

$LLVMLINK -o $RT_BITCODE_FILE $RT_BITCODE_DIR/*.bc
$LLVMDIS $RT_BITCODE_FILE -o $BUILD_DIR/${RT_BITCODE_NAME%.bc}.ll