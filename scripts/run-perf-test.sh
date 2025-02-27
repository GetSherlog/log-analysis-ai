#!/bin/bash
set -e

# Script to build and run the LogAI-CPP performance test in Docker

echo "===== Building and running LogAI-CPP performance test in Docker ====="

# Parse command line arguments
PARSER_TYPE="regex"  # Default parser type
INPUT_LOG_FILE=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --parser)
      PARSER_TYPE="$2"
      shift 2
      ;;
    --input)
      INPUT_LOG_FILE="$2"
      shift 2
      ;;
    *)
      INPUT_LOG_FILE="$1"
      shift
      ;;
  esac
done

# Validate parser type
if [[ "$PARSER_TYPE" != "csv" && "$PARSER_TYPE" != "json" && "$PARSER_TYPE" != "regex" && "$PARSER_TYPE" != "drain" ]]; then
  echo "Invalid parser type: $PARSER_TYPE"
  echo "Valid options are: csv, json, regex, drain"
  exit 1
fi

echo "Using parser type: $PARSER_TYPE"

# Check if Docker image exists, build if not
if [[ "$(docker images -q logai-cpp:latest 2> /dev/null)" == "" ]]; then
  echo "Building Docker image..."
  docker build -t logai-cpp:latest .
fi

# Create a volume for input/output files
mkdir -p ./test_data

# If no input file is specified, check for HDFS logs from Loghub
if [ -z "$INPUT_LOG_FILE" ]; then
  echo "No input file specified, checking for HDFS logs from Loghub..."
  
  # Directory for Loghub datasets
  LOGHUB_DIR="./test_data/loghub"
  HDFS_ZIP="${LOGHUB_DIR}/HDFS_v2.zip"
  # Updated path - HDFS logs are in node_logs directory
  HDFS_LOG_DIR="${LOGHUB_DIR}/node_logs"
  HDFS_LOG_FILE="${HDFS_LOG_DIR}/hadoop-hdfs-datanode-mesos-01.log"
  
  mkdir -p ${LOGHUB_DIR}
  
  # Check if we already have the file
  if [ ! -f "$HDFS_LOG_FILE" ]; then
    # Check if we've already downloaded the zip file
    if [ ! -f "$HDFS_ZIP" ]; then
      echo "Downloading HDFS logs from Loghub (Zenodo)..."
      curl -L -o ${HDFS_ZIP} https://zenodo.org/records/8196385/files/HDFS_v2.zip
    fi
    
    # Extract the zip file
    echo "Extracting HDFS logs..."
    unzip -q -o ${HDFS_ZIP} -d ${LOGHUB_DIR}
  fi
  
  # Use the HDFS log file for testing
  if [ -f "$HDFS_LOG_FILE" ]; then
    echo "Using HDFS log file: $HDFS_LOG_FILE"
    INPUT_LOG_FILE=$HDFS_LOG_FILE
  else
    echo "HDFS log file not found, falling back to sample log file."
  fi
fi

# If still no input file (HDFS download failed), generate a sample log file
if [ -z "$INPUT_LOG_FILE" ]; then
  echo "Creating a sample log file..."
  INPUT_LOG_FILE="./test_data/sample.log"
  
  if [ ! -f "$INPUT_LOG_FILE" ]; then
    echo "timestamp,level,message" > ${INPUT_LOG_FILE}
    for i in {1..10000}; do
      day=$(printf "%02d" $(( (i % 30) + 1 )))
      hour=$(printf "%02d" $(( i % 24 )))
      level=$([ $(( i % 5 )) -eq 0 ] && echo "ERROR" || [ $(( i % 3 )) -eq 0 ] && echo "WARNING" || echo "INFO")
      echo "2023-10-${day}T${hour}:00:00Z,${level},This is log message #$i with some sample content for testing." >> ${INPUT_LOG_FILE}
    done
    echo "Created sample log file with 10000 lines"
  fi
fi

# Define output file
OUTPUT_FILE="./test_data/output.parquet"

# Get the number of available CPUs (macOS compatible)
if [[ "$(uname)" == "Darwin" ]]; then
  NUM_CPUS=$(sysctl -n hw.ncpu)
else
  NUM_CPUS=$(nproc)
fi

# Clean the build directory to avoid CMake cache issues
echo "Cleaning build directory..."
rm -rf build

# Run the Docker container with the performance test
echo "Running performance test in Docker container..."
docker run --rm -v "$(pwd):/app" logai-cpp:latest bash -c "
  cd /app && \
  mkdir -p build && \
  cd build && \
  cmake .. && \
  make -j${NUM_CPUS} && \
  echo '===== Running Performance Test =====' && \
  ./src/perf_test --input /app/${INPUT_LOG_FILE#./} --output /app/${OUTPUT_FILE#./} --parser ${PARSER_TYPE}
"

# Check results
echo ""
echo "===== Test Results ====="
if [ -f "$OUTPUT_FILE" ]; then
  echo "Success! Parquet file created successfully."
  echo "Output Parquet file size: $(du -h ${OUTPUT_FILE} | cut -f1)"
else
  echo "Error: Parquet file was not created."
  exit 1
fi

echo ""
echo "Performance test completed!" 