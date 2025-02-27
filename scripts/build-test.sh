#!/bin/bash
# Build and run the large file performance test with preprocessing in Docker

# Check if Docker image exists, build if not
if [[ "$(docker images -q logai-cpp:latest 2> /dev/null)" == "" ]]; then
  echo "Building Docker image..."
  docker build -t logai-cpp:latest .
fi

# Clean build directory
echo "Cleaning build directory..."
rm -rf build
mkdir -p build

# Create test_data directory if it doesn't exist
mkdir -p test_data

# Build and run the performance test in Docker
echo "Building and running performance test with preprocessing..."
docker run --rm \
  -v "$(pwd):/app" \
  -w /app \
  -m 3g \
  --memory-swap 3g \
  logai-cpp:latest \
  bash -c "
    set -e
    mkdir -p build && cd build
    # Create a debuggable version of the CMakeLists.txt file with line numbers
    cat -n ../src/CMakeLists.txt > ../src/CMakeLists.txt.debug
    
    # Configure the build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    
    # Build only the large_perf_test target
    make -j$(nproc) large_perf_test
    
    # Run the test with preprocessing enabled
    cd ..
    ./build/src/large_perf_test --input ./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log --output ./test_data/output.parquet --parser drain --enable-preprocessing
  " 