#!/bin/bash

# Exit on error
set -e

# Build the project
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8 logbert_test
cd ..

# Check if test data exists, if not create a sample
SAMPLE_LOG_FILE="./test_data/sample_logs.txt"
mkdir -p test_data

if [ ! -f "$SAMPLE_LOG_FILE" ]; then
    echo "Creating sample log file..."
    cat > "$SAMPLE_LOG_FILE" << EOL
2023-01-15T12:34:56.789Z INFO [app.server] Server started at port 8080
2023-01-15T12:35:01.123Z DEBUG [app.connection] Client connected from 192.168.1.100
2023-01-15T12:35:02.456Z INFO [app.request] GET /api/users HTTP/1.1
2023-01-15T12:35:02.789Z DEBUG [app.db] Query executed in 54ms: SELECT * FROM users LIMIT 100
2023-01-15T12:35:03.012Z ERROR [app.handler] Failed to process request: Invalid user ID format
2023-01-15T12:35:03.345Z WARN [app.security] Rate limit exceeded for IP 192.168.1.100
2023-01-15T12:35:04.678Z INFO [app.response] Returned 400 Bad Request in 234ms
2023-01-15T12:35:10.901Z INFO [app.monitoring] Memory usage: 256MB, CPU: 23%
2023-01-15T12:35:15.234Z DEBUG [app.connection] Client disconnected: 192.168.1.100
2023-01-15T12:36:01.567Z INFO [app.server] Processing 5 active connections
2023-01-15T12:36:30.890Z INFO [app.cache] Cache hit ratio: 78.5%
2023-01-15T12:37:00.123Z WARN [app.disk] Free disk space below 20% on /var/log
2023-01-15T12:37:45.456Z ERROR [app.database] Connection to database lost after timeout of 30s
2023-01-15T12:38:01.789Z INFO [app.database] Reconnected to database after 3 retry attempts
2023-01-15T12:40:00.012Z INFO [app.backup] Starting scheduled backup of user data
2023-01-15T12:45:10.345Z INFO [app.backup] Backup completed, 1024 records processed
2023-01-15T12:50:00.678Z DEBUG [app.cleanup] Removed 50 expired session records
2023-01-15T13:00:00.901Z INFO [app.monitoring] System health check: OK
2023-01-15T13:15:30.234Z WARN [app.security] Failed login attempt for user 'admin' from 192.168.1.200
2023-01-15T13:16:45.567Z ERROR [app.security] Multiple failed login attempts detected, blocking IP 192.168.1.200 for 10 minutes
EOL
    echo "Created sample log file with 20 entries."
fi

# Run the LogBERT test with the sample log file
echo "Running LogBERT vectorizer test..."
./build/src/logbert_test "$SAMPLE_LOG_FILE" "./test_data/tokenizer_model.json"

echo "Test complete!" 