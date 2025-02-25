#!/bin/bash

# Build and run the Docker container in interactive mode
docker compose build 
docker compose run --rm logai-cpp 