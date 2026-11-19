#!/bin/bash
set -e

# Set variables
BENCHMARK_REPO="https://github.com/google/benchmark.git"
BENCHMARK_DIR="google_benchmark"
BUILD_DIR="build"

# Clone the Google Benchmark repository
if [ ! -d "$BENCHMARK_DIR" ]; then
    echo "Cloning Google Benchmark repository..."
    git clone --depth=1 $BENCHMARK_REPO $BENCHMARK_DIR
else
    echo "Google Benchmark repository already exists."
fi

# Create a build directory
mkdir -p $BENCHMARK_DIR/$BUILD_DIR
cd $BENCHMARK_DIR/$BUILD_DIR

# Build Google Benchmark
echo "Building Google Benchmark..."
cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_TESTING=OFF ..
make $1

# Check if the library was built successfully
if [ -f "src/libbenchmark.a" ]; then
    echo "Google Benchmark library (libbenchmark.a) built successfully."
else
    echo "Failed to build Google Benchmark library."
    exit 1
fi
