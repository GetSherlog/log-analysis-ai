#!/bin/bash

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting LogAI-CPP Web Server...${NC}"

# Create directories if they don't exist
echo -e "${YELLOW}Creating required directories...${NC}"
mkdir -p logs uploads

# Build and start the containers
echo -e "${YELLOW}Building and starting containers...${NC}"
docker-compose up --build -d

# Wait for services to be ready
echo -e "${YELLOW}Waiting for services to be healthy...${NC}"
attempt=1
max_attempts=10
service_ready=false

while [ $attempt -le $max_attempts ]; do
    echo -e "${YELLOW}Checking server health (attempt $attempt/$max_attempts)...${NC}"
    
    if curl -s http://localhost:8080/api/health > /dev/null; then
        service_ready=true
        break
    fi
    
    attempt=$((attempt+1))
    
    if [ $attempt -le $max_attempts ]; then
        echo -e "${YELLOW}Service not ready yet, waiting 5 seconds...${NC}"
        sleep 5
    fi
done

if [ "$service_ready" = false ]; then
    echo -e "${RED}Service did not become healthy within the expected time.${NC}"
    echo -e "${YELLOW}You can check the logs with:${NC} docker-compose logs -f"
    exit 1
fi

# Get the IP address based on OS
if [[ "$(uname)" == "Darwin" ]]; then
    # macOS
    ip_address="localhost"
else
    # Linux
    ip_address=$(hostname -I | awk '{print $1}')
fi

echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}LogAI-CPP Web Server is running!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Server URL:${NC} http://${ip_address}:8080/"
echo -e "${BLUE}Anomaly Detection Tool:${NC} http://${ip_address}:8080/anomaly_detection.html"
echo -e "${BLUE}API Documentation:${NC} http://${ip_address}:8080/web/swagger.json"
echo -e "\n${YELLOW}Management Commands:${NC}"
echo -e "  ${BLUE}View logs:${NC} docker-compose logs -f"
echo -e "  ${BLUE}Stop server:${NC} docker-compose down"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}" 