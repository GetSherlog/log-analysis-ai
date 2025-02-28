# Docker Setup for LogAI-CPP

This directory contains Docker configuration files to easily build and run the LogAI-CPP web server with the anomaly detection frontend.

## Prerequisites

- [Docker](https://docs.docker.com/get-docker/)
- [Docker Compose](https://docs.docker.com/compose/install/)

## Quick Start

The easiest way to start the LogAI-CPP web server is by using the provided script:

```bash
./start.sh
```

This script will:
1. Build the Docker image (if not already built)
2. Start the container
3. Wait for the service to be healthy
4. Display the URL to access the web interface

## Manual Setup

If you prefer to run the commands manually:

```bash
# Create required directories
mkdir -p logs uploads

# Build and start the containers
docker-compose up --build -d

# View logs
docker-compose logs -f

# Stop the service
docker-compose down
```

## Configuration

The default setup:
- Exposes the web server on port 8080
- Uses 16 worker threads
- Mounts `./logs` and `./uploads` directories for persistent storage

To change the port, edit the `docker-compose.yml` file and modify the port mapping.

## Accessing the Web Interface

After starting the container, you can access:

- Main page: http://localhost:8080/
- Anomaly Detection Tool: http://localhost:8080/anomaly_detection.html
- API Documentation: http://localhost:8080/web/swagger.json

## Troubleshooting

If you encounter any issues:

1. Check the logs:
   ```bash
   docker-compose logs -f
   ```

2. Ensure all dependencies are properly installed in the Dockerfile. If you need to add more dependencies, modify the Dockerfile and rebuild:
   ```bash
   docker-compose build --no-cache
   ```

3. Check if the port 8080 is already in use by another application. If so, modify the port mapping in `docker-compose.yml`.

4. Ensure Docker has enough resources allocated (memory, CPU). 