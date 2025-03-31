# Use an official Node runtime as a parent image
FROM node:20-alpine AS base

# Set working directory
WORKDIR /app

# Install dependencies
COPY package*.json ./
RUN npm install

# Copy the rest of the application code
COPY . .

# Set environment variables
ENV NEXT_PUBLIC_API_URL=http://logai-api:8000

# Build the Next.js application
RUN npm run build

# Expose port 3000
EXPOSE 3000

# Command to run the application
CMD ["npm", "start"] 