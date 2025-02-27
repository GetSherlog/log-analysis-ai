#!/bin/bash

# Exit on error
set -e

# Set variables
BUILD_DIR="build"
LOG_FILE="test_data/sample.log"

# Check if test file exists, if not create a simple one for testing
if [ ! -f "$LOG_FILE" ]; then
    echo "Creating sample log file for testing..."
    mkdir -p test_data
    cat <<EOF > "$LOG_FILE"
2023-04-12T12:34:56Z [INFO] Connection from 192.168.1.100 to server-01 established
2023-04-12T12:35:23Z [ERROR] Failed to authenticate user john.doe@example.com (session: 3a7c8d9e-1234-5678-90ab-cdef01234567)
2023-04-12T12:36:10Z [WARN] Disk usage at 85% on /dev/sda1
2023-04-12T12:37:45Z [INFO] Process started with PID 12345 on host server-02
2023-04-12T12:38:22Z [DEBUG] Request from 10.0.0.5 processed in 123ms
2023-04-12T12:39:11Z [ERROR] Database connection timeout after 30s (id: 987654321)
2023-04-12T12:40:00Z [INFO] User admin logged in from 172.16.254.1
2023-04-12T12:41:15Z [WARN] Memory usage critical: 95% (16GB/17GB)
2023-04-12T12:42:30Z [INFO] Task d7e6f5d4-abcd-efgh-ijkl-mnop76543210 completed successfully
2023-04-12T12:43:45Z [ERROR] Unable to connect to service at https://api.example.com/v2/resource (error code: 503)
EOF
    echo "Sample log file created at $LOG_FILE"
fi

# Build the project if build directory doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory and building project..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ..
    make -j$(nproc)
    cd ..
else
    echo "Rebuilding project..."
    cd "$BUILD_DIR"
    make -j$(nproc)
    cd ..
fi

# Run the preprocessor test
echo "Running preprocessor test..."
"$BUILD_DIR/src/preprocessor_test" "$LOG_FILE"

echo
echo "Test completed!" 