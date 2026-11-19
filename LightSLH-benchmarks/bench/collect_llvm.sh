#!/bin/bash

# Exit on any error
set -e

sysOS=`uname -s`
MacLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.4/clang+llvm-14.0.4-x86_64-apple-darwin.tar.xz"
UbuntuLLVM="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz"
LLVMHome="llvm-14.0.4.obj"

function generic_download_file {
    if [ $# -ne 2 ]
    then
        echo "$0: bad args to generic_download_file!"
        exit 1
    fi

    if [[ "$sysOS" == "Darwin" ]]
    then
        echo "Downloading Mac LLVM ..."
        curl -L "$1" -o "$2"
    elif [[ "$sysOS" == "Linux" ]]
    then
        echo "Downloading Linux LLVM ..."
        wget -c "$1" -O "$2"
    else
        echo "Cannot download file for non-Darwin/-Linux."
    fi
}

# OS-specific values.
urlLLVM=""
OSDisplayName=""

if [[ $sysOS == "Darwin" ]]
then
    urlLLVM="$MacLLVM"
    OSDisplayName="macOS"
elif [[ $sysOS == "Linux" ]]
then
    urlLLVM="$UbuntuLLVM"
    OSDisplayName="Ubuntu"
else
    echo "Builds outside Ubuntu and macOS are not supported."
fi

echo "Downloading LLVM binary for $OSDisplayName"
generic_download_file "$urlLLVM" llvm.tar.xz
mkdir -p "./$LLVMHome" && tar -xf llvm.tar.xz -C "./$LLVMHome" --strip-components 1
rm llvm.tar.xz

# The original LightSLH build llvm from source, we download pre-built binaries to save time.
# # Define LLVM version and installation directory
# CUR_PATH=$(pwd)
# INSTALL_DIR=$(pwd)/llvm-project-14.0.4.src/install

# # Download LLVM source code
# echo "Downloading LLVM 14.0.4..."
# wget https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.4/llvm-project-14.0.4.src.tar.xz
# # Extract the downloaded source code
# echo "Extracting LLVM source code..."
# tar -xf llvm-project-14.0.4.src.tar.xz
# rm  llvm-project-14.0.4.src.tar.xz

# # Create a build directory
# echo "Creating build directory..."
# mkdir -p llvm-project-14.0.4.src/build
# cd llvm-project-14.0.4.src/build

# # Configure the build with CMake
# echo "Configuring the build with CMake..."
# cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DCMAKE_BUILD_TYPE=RelWithDebInfo  -DLLVM_ENABLE_ASSERTIONS=On -DLLVM_ENABLE_PROJECTS='clang;lld' -DLLVM_TARGETS_TO_BUILD='X86' ../llvm

# # Compile and install LLVM
# echo "Compiling and installing LLVM..."
# make $1
# make install