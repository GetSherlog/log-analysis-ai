#!/bin/bash

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting LogAI-CPP Web Server with Next.js UI...${NC}"

# Create directories if they don't exist
echo -e "${YELLOW}Creating required directories...${NC}"
mkdir -p logs uploads src/web_server/web

# Make sure we have the index.html file
if [ ! -f "src/web_server/web/index.html" ]; then
    echo -e "${YELLOW}Creating a sample index.html file...${NC}"
    cat > src/web_server/web/index.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>LogAI-CPP Web Server</title>
</head>
<body>
    <h1>LogAI-CPP Web Server</h1>
    <p>Server is running!</p>
</body>
</html>
EOF
fi

# Check if Next.js UI directory exists
if [ ! -d "logai-next-ui" ]; then
    echo -e "${RED}Error: logai-next-ui directory not found!${NC}"
    echo -e "${YELLOW}Make sure you have the Next.js UI set up in the logai-next-ui directory.${NC}"
    exit 1
fi

# Build and start the containers
echo -e "${YELLOW}Building and starting containers...${NC}"
docker-compose up --build -d

# Wait for services to be ready
echo -e "${YELLOW}Waiting for services to be healthy...${NC}"
attempt=1
max_attempts=20
service_ready=false

while [ $attempt -le $max_attempts ]; do
    echo -e "${YELLOW}Checking backend server health (attempt $attempt/$max_attempts)...${NC}"
    
    if curl -s http://localhost:8080/health > /dev/null; then
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

# Check if Next.js UI is up
echo -e "${YELLOW}Checking Next.js UI status...${NC}"
attempt=1
max_attempts=10
ui_ready=false

while [ $attempt -le $max_attempts ]; do
    echo -e "${YELLOW}Checking Next.js UI (attempt $attempt/$max_attempts)...${NC}"
    
    if curl -s http://localhost:3000 > /dev/null; then
        ui_ready=true
        break
    fi
    
    attempt=$((attempt+1))
    
    if [ $attempt -le $max_attempts ]; then
        echo -e "${YELLOW}Next.js UI not ready yet, waiting 5 seconds...${NC}"
        sleep 5
    fi
done

# Get the IP address based on OS
if [[ "$(uname)" == "Darwin" ]]; then
    # macOS
    ip_address="localhost"
else
    # Linux
    ip_address=$(hostname -I | awk '{print $1}')
fi

echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}LogAI-CPP with Next.js UI is running!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}Backend API:${NC} http://${ip_address}:8080/"

if [ "$ui_ready" = true ]; then
    echo -e "${BLUE}Next.js UI:${NC} http://${ip_address}:3000/"
else
    echo -e "${YELLOW}Next.js UI is not yet responding. You can check the logs for issues:${NC}"
    echo -e "  ${BLUE}View Next.js logs:${NC} docker-compose logs -f logai-nextjs-ui"
fi

echo -e "${BLUE}Health Check:${NC} http://${ip_address}:8080/health"
echo -e "\n${YELLOW}Management Commands:${NC}"
echo -e "  ${BLUE}View all logs:${NC} docker-compose logs -f"
echo -e "  ${BLUE}View backend logs:${NC} docker-compose logs -f logai-web"
echo -e "  ${BLUE}View UI logs:${NC} docker-compose logs -f logai-nextjs-ui"
echo -e "  ${BLUE}Stop services:${NC} docker-compose down"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}" 