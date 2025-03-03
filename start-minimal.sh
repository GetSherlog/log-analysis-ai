#!/bin/bash

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting LogAI-CPP Minimal Web Server...${NC}"

# Build and start the containers
echo -e "${YELLOW}Building and starting minimal container...${NC}"
docker-compose -f docker-compose.minimal.yml up --build -d

# Wait for services to be ready
echo -e "${YELLOW}Waiting for service to be healthy...${NC}"
attempt=1
max_attempts=10
service_ready=false

while [ $attempt -le $max_attempts ]; do
    echo -e "${YELLOW}Checking server health (attempt $attempt/$max_attempts)...${NC}"
    
    if curl -s http://localhost:8080/health > /dev/null; then
        service_ready=true
        break
    fi
    
    attempt=$((attempt+1))
    
    if [ $attempt -le $max_attempts ]; then
        echo -e "${YELLOW}Service not ready yet, waiting 2 seconds...${NC}"
        sleep 2
    fi
done

if [ "$service_ready" = false ]; then
    echo -e "${RED}Service did not become healthy within the expected time.${NC}"
    echo -e "${YELLOW}You can check the logs with:${NC} docker-compose -f docker-compose.minimal.yml logs -f"
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
echo -e "${GREEN}LogAI-CPP Minimal Web Server is running!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Server URL:${NC} http://${ip_address}:8080/"
echo -e "${BLUE}Health Check:${NC} http://${ip_address}:8080/health"
echo -e "\n${YELLOW}Management Commands:${NC}"
echo -e "  ${BLUE}View logs:${NC} docker-compose -f docker-compose.minimal.yml logs -f"
echo -e "  ${BLUE}Stop server:${NC} docker-compose -f docker-compose.minimal.yml down"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"