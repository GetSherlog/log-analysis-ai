FROM ubuntu:22.04 AS dependencies

# Avoid prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install basic dependencies
RUN apt-get update && apt-get install -y \
    git cmake g++ make libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev libmariadb-dev wget curl \
    libcurl4-openssl-dev \
    libboost-all-dev python3 ninja-build pkg-config unzip \
    python3-pip nlohmann-json3-dev libjemalloc-dev \
    libgoogle-glog-dev libgflags-dev liblz4-dev libleveldb-dev \
    libtbb-dev libhiredis-dev libspdlog-dev libfmt-dev \
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

# Install FastFloat (needed for Folly)
WORKDIR /tmp/fastfloat
RUN git clone https://github.com/fastfloat/fast_float.git \
    && cd fast_float \
    && mkdir build && cd build \
    && cmake .. \
    && make install

# Install gflags from source (ensuring shared libraries are built)
WORKDIR /tmp/gflags
RUN git clone https://github.com/gflags/gflags.git \
    && cd gflags \
    && mkdir build && cd build \
    && cmake -DBUILD_SHARED_LIBS=ON \
            -DBUILD_STATIC_LIBS=ON \
            -DBUILD_gflags_LIB=ON \
            -DINSTALL_HEADERS=ON \
            -DINSTALL_SHARED_LIBS=ON \
            -DINSTALL_STATIC_LIBS=ON .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig \
    # Create symlink for libgflags_shared if needed
    && if [ -f /usr/local/lib/libgflags.so ] && [ ! -f /usr/local/lib/libgflags_shared.so ]; then \
           ln -s /usr/local/lib/libgflags.so /usr/local/lib/libgflags_shared.so; \
       fi

# Install Folly
WORKDIR /tmp/folly
RUN apt-get update && apt-get install -y \
    autoconf automake binutils-dev cmake libdwarf-dev \
    libevent-dev libsodium-dev libtool ninja-build \
    libgoogle-glog-dev libboost-all-dev \
    libdouble-conversion-dev liblz4-dev \
    liblzma-dev libzstd-dev libbz2-dev libsnappy-dev \
    python3-pip python3-dev zlib1g-dev zstd pkg-config \
    libfmt-dev libunwind-dev libicu-dev \
    # Additional dependencies commonly needed
    libfmt-dev libunwind-dev libicu-dev \
    && rm -rf /var/lib/apt/lists/* \
    && git clone https://github.com/facebook/folly && cd folly/build \
    && cmake -DBUILD_SHARED_LIBS=ON \
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
            -DBUILD_EXAMPLES=OFF \
            -DBUILD_CTL=OFF \
            -DBUILD_TESTING=OFF .. \
    && make all \
    && make install \
    && ldconfig

# Install llama.cpp
WORKDIR /tmp/llama
RUN git clone https://github.com/ggerganov/llama.cpp.git \
    && cd llama.cpp \
    && mkdir build && cd build \
    && cmake -DBUILD_SHARED_LIBS=ON \
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
            -DLLAMA_NATIVE=OFF \
            -DLLAMA_BUILD_EXAMPLES=OFF \
            -DLLAMA_BUILD_TESTS=OFF .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# Install DuckDB
WORKDIR /tmp/duckdb
RUN git clone https://github.com/duckdb/duckdb.git \
    && cd duckdb \
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

# Build LogAI library
RUN mkdir -p build && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release .. \
    && make -j$(nproc) \
    && make install

# Final stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl-dev zlib1g-dev libjsoncpp-dev uuid-dev \
    libmariadb-dev libboost-all-dev libjemalloc-dev \
    libgoogle-glog-dev libgflags-dev liblz4-dev \
    libleveldb-dev libtbb-dev curl libcurl4-openssl-dev libhiredis-dev libspdlog-dev libfmt-dev \
    libdouble-conversion-dev liblzma-dev libzstd-dev libbz2-dev libsnappy-dev \
    libfmt-dev libspdlog-dev libsodium-dev libsodium23 libunwind8 libunwind-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy all the libraries from the build stage
COPY --from=build /usr/local/lib/* /usr/local/lib/
COPY --from=build /usr/local/include /usr/local/include

# Update the dynamic linker run-time bindings
RUN ldconfig

# Create necessary directories
RUN mkdir -p /var/log/logai /var/uploads/logai \
    && chmod -R 777 /var/log/logai \
    && chmod -R 777 /var/uploads/logai

# Copy built artifacts
COPY --from=build /app/build/bin/logai /usr/local/bin/

# Set permissions
RUN chmod +x /usr/local/bin/logai

# Set up environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/lib:$LD_LIBRARY_PATH

# Create working directory
WORKDIR /app
COPY --from=build /app/build /app/build
RUN mkdir -p /app/uploads /app/logs

# Startup script
RUN echo '#!/bin/bash\necho "LogAI CLI is available."\necho "Run: logai --help for usage information."\nexec /bin/bash\n' > /start.sh && chmod +x /start.sh

# Use a shell script
CMD ["/start.sh"] 