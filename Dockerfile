FROM ubuntu:22.04 AS build

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    make \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev \
    libmariadb-dev \
    wget \
    curl \
    libboost-all-dev \
    python3 \
    ninja-build \
    pkg-config \
    unzip \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install Apache Arrow
RUN apt-get update && apt-get install -y \
    libboost-all-dev \
    libjemalloc-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    liblz4-dev \
    libleveldb-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Apache Arrow from source
WORKDIR /tmp
RUN wget -q https://github.com/apache/arrow/archive/refs/tags/apache-arrow-12.0.0.tar.gz && \
    tar -xf apache-arrow-12.0.0.tar.gz && \
    cd arrow-apache-arrow-12.0.0 && \
    mkdir cpp/build && cd cpp/build && \
    cmake -DARROW_PARQUET=ON -DARROW_DATASET=ON -DARROW_CSV=ON .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Install Eigen
RUN wget -q https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz && \
    tar -xf eigen-3.4.0.tar.gz && \
    cd eigen-3.4.0 && \
    mkdir build && cd build && \
    cmake .. && \
    make install

# Install TBB
RUN apt-get update && apt-get install -y \
    libtbb-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon framework
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v1.8.6 && \
    mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Create a working directory
WORKDIR /app

# Copy the source code
COPY . .

# Build LogAI-CPP
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    zlib1g \
    libjsoncpp25 \
    uuid-runtime \
    libmariadb3 \
    libboost-program-options1.74.0 \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libgoogle-glog0v5 \
    libgflags2.2 \
    liblz4-1 \
    libleveldb1d \
    libtbb12 \
    && rm -rf /var/lib/apt/lists/*

# Copy the built executable and necessary files from the build stage
COPY --from=build /app/build/logai_web_server /usr/local/bin/
COPY --from=build /app/build/lib/ /usr/local/lib/
COPY --from=build /app/src/web/templates /usr/local/share/logai/templates
COPY --from=build /app/src/web/static /usr/local/share/logai/static
COPY --from=build /usr/local/lib/libdrogon* /usr/local/lib/
COPY --from=build /usr/local/lib/libarrow* /usr/local/lib/
COPY --from=build /usr/local/lib/libparquet* /usr/local/lib/

# Update library cache
RUN ldconfig

# Create directories for logs and uploads
RUN mkdir -p /app/logs /app/uploads

# Set working directory
WORKDIR /app

# Set environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
ENV PATH=/usr/local/bin:$PATH

# Expose the web server port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:8080/api/health || exit 1

# Start the web server with 16 threads
CMD ["/usr/local/bin/logai_web_server", "--threads", "16", "--document-root", "/usr/local/share/logai"] 