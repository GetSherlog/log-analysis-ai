#!/bin/bash

# Enhanced Performance Test Script for Large Log Files in LogAI-CPP
# This script handles large datasets with memory-adaptive chunking
# SIMD DISABLED VERSION FOR PERFORMANCE COMPARISON

# Default values
PARSER_TYPE="drain"
INPUT_FILE=""
OUTPUT_FILE="./test_data/output-no-simd.parquet"
MEMORY_LIMIT="3g"
CHUNK_SIZE="10000"  # Changed from 0 to a reasonable default value
ENABLE_PREPROCESSING=false  # Preprocessing disabled by default

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --parser)
      PARSER_TYPE="$2"
      shift 2
      ;;
    --input)
      INPUT_FILE="$2"
      shift 2
      ;;
    --output)
      OUTPUT_FILE="$2"
      shift 2
      ;;
    --memory)
      MEMORY_LIMIT="$2"
      shift 2
      ;;
    --chunk-size)
      CHUNK_SIZE="$2"
      shift 2
      ;;
    --preprocess)
      ENABLE_PREPROCESSING=true
      shift
      ;;
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --parser TYPE      Parser type (csv, json, drain, regex) [default: drain]"
      echo "  --input FILE       Input log file path (if not provided, uses default HDFS logs)"
      echo "  --output FILE      Output Parquet file path [default: ./test_data/output-no-simd.parquet]"
      echo "  --memory LIMIT     Memory limit for Docker container (e.g. 3g, 4g) [default: 3g]"
      echo "  --chunk-size SIZE  Size of chunks in bytes (0 = auto) [default: 0]"
      echo "  --preprocess       Enable log preprocessing to normalize log entries"
      echo "  --help             Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Print configuration
echo "===== Enhanced LogAI-CPP Performance Test for Large Files (NO SIMD) ====="
echo "Using parser type: $PARSER_TYPE"
echo "Memory limit: $MEMORY_LIMIT"
if [ "$ENABLE_PREPROCESSING" = true ]; then
  echo "Preprocessing: Enabled"
else
  echo "Preprocessing: Disabled"
fi
echo "SIMD: DISABLED"

# Check if input file is specified, otherwise use default HDFS logs
if [ -z "$INPUT_FILE" ]; then
  echo "No input file specified, using default HDFS logs from Loghub"
  if [ ! -d "./test_data/loghub/node_logs" ]; then
    echo "Downloading HDFS logs from Loghub..."
    mkdir -p ./test_data/loghub/node_logs
    curl -o ./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log https://raw.githubusercontent.com/logpai/loghub/master/HDFS/HDFS_2/hadoop-hdfs-datanode-mesos-01.log
  fi
  INPUT_FILE="./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log"
fi

echo "Input file: $INPUT_FILE"
echo "Output file: $OUTPUT_FILE"

# Create the medium sample log file if it doesn't exist
if [ "$INPUT_FILE" = "test_data/medium_sample.log" ] && [ ! -f "$INPUT_FILE" ]; then
  echo "Creating medium sample log file..."
  if [ -f "./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log" ]; then
    # Create a 10MB sample from the HDFS log
    head -n 50000 ./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log > "$INPUT_FILE"
  else
    # Download the HDFS log and create a sample
    mkdir -p ./test_data/loghub/node_logs
    curl -o ./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log https://raw.githubusercontent.com/logpai/loghub/master/HDFS/HDFS_2/hadoop-hdfs-datanode-mesos-01.log
    head -n 50000 ./test_data/loghub/node_logs/hadoop-hdfs-datanode-mesos-01.log > "$INPUT_FILE"
  fi
  echo "Created medium sample log file at $INPUT_FILE"
fi

# Check if Docker image exists, build if not
if [[ "$(docker images -q logai-cpp:latest 2> /dev/null)" == "" ]]; then
  echo "Building Docker image..."
  docker build -t logai-cpp:latest .
fi

# Clean build directory
echo "Cleaning build directory..."
rm -rf build

# Create test_data directory if it doesn't exist
mkdir -p test_data

# Get the number of available CPUs
if [[ "$(uname)" == "Darwin" ]]; then
  NUM_CPUS=$(sysctl -n hw.ncpu)
else
  NUM_CPUS=$(nproc)
fi

# Build and run the performance test in Docker with the specified memory limit
echo "Building and running performance test with large file processing (NO SIMD)..."
docker run --rm \
  -v "$(pwd):/app" \
  -w /app \
  -m $MEMORY_LIMIT \
  --memory-swap $MEMORY_LIMIT \
  logai-cpp:latest \
  bash -c "
    set -e
    mkdir -p build && cd build
    
    # Configure with SIMD disabled
    echo 'Configuring CMake with SIMD explicitly disabled'
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=ON -DDISABLE_SIMD=1 .. || (echo 'CMake configuration failed' && exit 1)
    
    # Build only the large_perf_test target
    make -j${NUM_CPUS} large_perf_test
    cd ..
    echo '===== Running Large File Performance Test (NO SIMD) ====='
    
    # Run the large performance test with the provided parameters
    START_TIME=\$(date +%s%N)
    ./build/src/large_perf_test --input $INPUT_FILE --output $OUTPUT_FILE --parser $PARSER_TYPE --memory-limit $((${MEMORY_LIMIT%g} * 1024)) --chunk-size $CHUNK_SIZE $([ "$ENABLE_PREPROCESSING" = true ] && echo "--enable-preprocessing")
    END_TIME=\$(date +%s%N)
    DURATION=\$((\$END_TIME - \$START_TIME))
    DURATION_MS=\$((\$DURATION / 1000000))
    
    # Check if the performance test completed successfully
    if [ \$? -eq 0 ]; then
      echo 'Performance test completed successfully!'
      echo 'Total execution time: '\$DURATION_MS' ms'
      ls -lh $OUTPUT_FILE
      echo 'Output Parquet file size: '$(du -h $OUTPUT_FILE | cut -f1)
    else
      echo 'Performance test failed!'
      exit 1
    fi
  "

# Check if the docker command completed successfully
if [ $? -eq 0 ]; then
  echo "Large file performance test (NO SIMD) completed successfully!"
else
  echo "Large file performance test (NO SIMD) failed!"
  exit 1
fi 