#!/bin/bash

# Script to build and run the feature extractor test

set -e  # Exit on any error

echo "========== Building feature extractor test =========="

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target feature_extractor_test -- -j$(nproc)

echo ""
echo "========== Running feature extractor test =========="
echo ""

# Run the test
./src/feature_extractor_test

echo ""
echo "========== Feature extractor test completed ==========" 