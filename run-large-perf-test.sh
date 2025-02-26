#!/bin/bash

# Enhanced Performance Test Script for Large Log Files in LogAI-CPP
# This script handles large datasets with memory-adaptive chunking

# Default values
PARSER_TYPE="drain"
INPUT_FILE=""
OUTPUT_FILE="./test_data/output.parquet"
MEMORY_LIMIT="3g"
CHUNK_SIZE="0"  # 0 means auto-determine based on memory

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
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --parser TYPE      Parser type (csv, json, drain, regex) [default: drain]"
      echo "  --input FILE       Input log file path (if not provided, uses default HDFS logs)"
      echo "  --output FILE      Output Parquet file path [default: ./test_data/output.parquet]"
      echo "  --memory LIMIT     Memory limit for Docker container (e.g. 3g, 4g) [default: 3g]"
      echo "  --chunk-size SIZE  Size of chunks in bytes (0 = auto) [default: 0]"
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
echo "===== Enhanced LogAI-CPP Performance Test for Large Files ====="
echo "Using parser type: $PARSER_TYPE"
echo "Memory limit: $MEMORY_LIMIT"

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
echo "Building and running performance test with large file processing..."
docker run --rm \
  -v "$(pwd):/app" \
  -w /app \
  -m $MEMORY_LIMIT \
  --memory-swap $MEMORY_LIMIT \
  logai-cpp:latest \
  bash -c "
    set -e
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j${NUM_CPUS}
    cd ..
    echo '===== Running Large File Performance Test ====='
    
    # Run the large performance test with the provided parameters
    ./build/src/large_perf_test --input $INPUT_FILE --output $OUTPUT_FILE --parser $PARSER_TYPE --memory-limit $((${MEMORY_LIMIT%g} * 1024)) --chunk-size $CHUNK_SIZE
    
    # Check if the performance test completed successfully
    if [ \$? -eq 0 ]; then
      echo 'Performance test completed successfully!'
      ls -lh $OUTPUT_FILE
      echo 'Output Parquet file size: '$(du -h $OUTPUT_FILE | cut -f1)
    else
      echo 'Performance test failed!'
      exit 1
    fi
  "

# Check if the docker command completed successfully
if [ $? -eq 0 ]; then
  echo "Large file performance test completed successfully!"
else
  echo "Large file performance test failed!"
  exit 1
fi
