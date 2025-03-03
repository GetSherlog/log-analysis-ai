FROM ubuntu:22.04 AS dependencies

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install basic dependencies
RUN apt-get update && apt-get install -y \
    git cmake g++ make libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev libmariadb-dev wget curl \
    libboost-all-dev python3 ninja-build pkg-config unzip \
    python3-pip nlohmann-json3-dev libjemalloc-dev \
    libgoogle-glog-dev libgflags-dev liblz4-dev libleveldb-dev \
    libtbb-dev libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Apache Arrow
WORKDIR /tmp/arrow
RUN wget -q https://github.com/apache/arrow/archive/refs/tags/apache-arrow-12.0.0.tar.gz \
    && tar -xf apache-arrow-12.0.0.tar.gz \
    && cd arrow-apache-arrow-12.0.0/cpp \
    && mkdir build && cd build \
    && cmake -DARROW_PARQUET=ON -DARROW_DATASET=ON -DARROW_CSV=ON .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# Install Eigen
WORKDIR /tmp/eigen
RUN wget -q https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz \
    && tar -xf eigen-3.4.0.tar.gz \
    && cd eigen-3.4.0 \
    && mkdir build && cd build \
    && cmake .. \
    && make install

# Install Abseil
WORKDIR /tmp/abseil
RUN git clone https://github.com/abseil/abseil-cpp.git \
    && cd abseil-cpp \
    && git checkout 20230125.3 \
    && mkdir build && cd build \
    && cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
            -DCMAKE_CXX_STANDARD=17 \
            -DABSL_ENABLE_INSTALL=ON \
            -DABSL_PROPAGATE_CXX_STD=ON .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# Install Drogon
WORKDIR /tmp/drogon
RUN git clone https://github.com/drogonframework/drogon \
    && cd drogon \
    && git checkout v1.8.6 \
    && git submodule update --init --recursive \
    && mkdir build && cd build \
    && cmake -DBUILD_SHARED_LIBS=ON \
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
            -DBUILD_EXAMPLES=OFF \
            -DBUILD_CTL=OFF \
            -DBUILD_TESTING=OFF .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# Verify that the library is installed correctly
RUN ls -la /usr/local/lib/libdrogon* || echo "Drogon library not found!"

# Build stage
FROM dependencies AS build

WORKDIR /app
COPY . .

# Build LogAI library and web server
RUN mkdir -p build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make -j$(nproc) \
    && make install \
    && if [ ! -f bin/logai_web_server ]; then \
           echo "Error: Web server build failed" && exit 1; \
       fi

# Final stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl-dev zlib1g-dev libjsoncpp-dev uuid-dev \
    libmariadb-dev libboost-all-dev libjemalloc-dev \
    libgoogle-glog-dev libgflags-dev liblz4-dev \
    libleveldb-dev libtbb-dev curl libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy all the libraries from the build stage
COPY --from=build /usr/local/lib/* /usr/local/lib/
COPY --from=build /usr/local/include /usr/local/include

# Create symlinks if needed and update the dynamic linker run-time bindings
RUN mkdir -p /usr/lib/drogon && \
    ln -sf /usr/local/lib/libdrogon.so /usr/lib/libdrogon.so && \
    ln -sf /usr/local/lib/libdrogon.so /usr/lib/drogon/libdrogon.so && \
    ldconfig

# Create necessary directories
RUN mkdir -p /var/log/logai /var/uploads/logai \
    && chmod -R 777 /var/log/logai \
    && chmod -R 777 /var/uploads/logai

# Copy built artifacts and web assets
COPY --from=build /app/build/bin/logai_web_server /usr/local/bin/

# Create web directory structure
RUN mkdir -p /app/src/web_server/web

# Create a basic index.html
RUN echo '<!DOCTYPE html>\n<html>\n<head>\n<title>LogAI-CPP Web Server</title>\n</head>\n<body>\n<h1>LogAI-CPP Web Server</h1>\n<p>Server is running!</p>\n<p><a href="/health">Health Check</a></p>\n</body>\n</html>' > /app/src/web_server/web/index.html

# Set permissions
RUN chmod +x /usr/local/bin/logai_web_server

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8080/health || exit 1

# Start the web server with proper working directory
WORKDIR /app
COPY --from=build /app/build /app/build
RUN mkdir -p /app/uploads

# Set up environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/lib:$LD_LIBRARY_PATH

# Debugging script
RUN echo '#!/bin/bash\necho "Starting LogAI Web Server..."\ncd /app\nexec /usr/local/bin/logai_web_server\n' > /start.sh && chmod +x /start.sh

# Use a shell script and set the right working directory
CMD ["/start.sh"] 