#!/bin/bash

# Exit on error
set -e

# Build the project
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8 logbert_unit_test
cd ..

# Run the LogBERT unit tests
echo "Running LogBERT unit tests..."
./build/src/logbert_unit_test

echo "Unit tests complete!" 