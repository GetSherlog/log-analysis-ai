# LogAI-CPP Web Server

The LogAI-CPP Web Server provides a high-performance HTTP API to interact with the LogAI-CPP library functionality. It is built using the [Drogon](https://github.com/drogonframework/drogon) C++ web framework, which is one of the fastest C++ web frameworks available.

## Features

- RESTful API for all LogAI-CPP functionality
- JSON-based request/response format
- Highly concurrent request handling
- Optimized for performance with multi-threading
- Swagger/OpenAPI documentation

## Building and Running

### Prerequisites

- C++17 compatible compiler
- CMake 3.10+
- Drogon dependencies (OpenSSL, zlib, etc.)
- All LogAI-CPP library dependencies

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/your-org/logai-cpp.git
cd logai-cpp

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run the server (default port 8080)
./src/web_server/logai_server

# Run with custom port and thread count
./src/web_server/logai_server 9090 32
```

### Docker

You can also run the LogAI-CPP Web Server using Docker:

```bash
# Build the Docker image
docker build -t logai-cpp-server .

# Run the container
docker run -p 8080:8080 logai-cpp-server
```

## API Documentation

The web server includes a Swagger/OpenAPI documentation that can be accessed at `http://localhost:8080/web/swagger.html` when the server is running.

### Available Endpoints

#### Health and Information

- `GET /health` - Check server health
- `GET /api` - Get API information

#### Log Parsing

- `POST /api/parser/drain` - Parse log lines using DRAIN algorithm
- `POST /api/parser/file` - Parse logs from a file

#### Feature Extraction

- `POST /api/features/extract` - Extract features from log lines
- `POST /api/features/logbert` - Vectorize log messages using LogBERT

#### Anomaly Detection

- `POST /api/anomalies/ocsvm` - Detect anomalies using One-Class SVM
- `POST /api/anomalies/dbscan` - Cluster log events using DBSCAN

## Example Usage

### Parse logs using DRAIN algorithm

```bash
curl -X POST http://localhost:8080/api/parser/drain \
  -H "Content-Type: application/json" \
  -d '{
    "logLines": [
      "User login failed for user john",
      "User login failed for user alice",
      "Connection timeout after 30 seconds",
      "Connection timeout after 45 seconds",
      "Database query execution took 15ms"
    ],
    "depth": 4,
    "similarityThreshold": 0.5
  }'
```

### Extract features from log lines

```bash
curl -X POST http://localhost:8080/api/features/extract \
  -H "Content-Type: application/json" \
  -d '{
    "logLines": [
      "Error: Connection refused at line 52",
      "Warning: Disk usage at 85%",
      "Error: Invalid argument at line 127"
    ]
  }'
```

### Detect anomalies using One-Class SVM

```bash
curl -X POST http://localhost:8080/api/anomalies/ocsvm \
  -H "Content-Type: application/json" \
  -d '{
    "featureVectors": [
      [0.1, 0.2, 0.3],
      [0.2, 0.3, 0.4],
      [0.3, 0.4, 0.5],
      [1.5, 1.6, 1.7]
    ],
    "kernel": "rbf",
    "nu": 0.1,
    "gamma": 0.1
  }'
```

## Performance Considerations

The LogAI-CPP Web Server is designed for high performance:

- Uses Drogon's asynchronous model for handling multiple requests
- Configurable thread pool size for parallel request processing
- Uses C++ optimized code throughout the stack
- Minimizes data copying between components
- Leverages LogAI-CPP's optimized algorithms and SIMD operations

## Security Considerations

When deploying the LogAI-CPP Web Server in production:

- Configure proper authentication and authorization
- Limit file access to specific directories
- Set appropriate CORS restrictions in the configuration
- Run behind a reverse proxy like Nginx for additional security
- Use TLS/SSL for encrypted communications 