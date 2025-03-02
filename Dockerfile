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
    libtbb-dev \
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
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

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
    libleveldb-dev libtbb-dev curl \
    && rm -rf /var/lib/apt/lists/*

# Create necessary directories
RUN mkdir -p /var/log/logai /var/uploads/logai \
    && chmod -R 777 /var/log/logai \
    && chmod -R 777 /var/uploads/logai

# Copy built artifacts and web assets
COPY --from=build /app/build/bin/logai_web_server /usr/local/bin/
COPY --from=build /app/src/web_server/web /usr/local/share/logai/

# Set permissions
RUN chmod +x /usr/local/bin/logai_web_server

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8080/api/health || exit 1

# Start the web server
CMD ["/usr/local/bin/logai_web_server", "--threads", "16", "--document-root", "/usr/local/share/logai"] 