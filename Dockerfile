FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Install basic dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    pkg-config \
    libssl-dev \
    vim \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install Apache Arrow and Parquet
RUN apt-get update && apt-get install -y \
    apt-transport-https \
    lsb-release \
    gnupg \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    wget -O - https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb -O /tmp/apache-arrow-apt-source.deb && \
    apt-get update && apt-get install -y /tmp/apache-arrow-apt-source.deb && \
    apt-get update && apt-get install -y \
    libarrow-dev \
    libarrow-dataset-dev \
    libparquet-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install nlohmann-json
RUN apt-get update && apt-get install -y \
    nlohmann-json3-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Show information about the Arrow installation
RUN echo "Arrow headers location:" && \
    find /usr/include -name "arrow" -type d | xargs ls -la && \
    echo "Arrow version installed:" && \
    apt-cache show libarrow-dev | grep -E "Version|Depends"

# Set up working directory
WORKDIR /app

# The directory will be mounted from the host
VOLUME ["/app"]

# Set the default command
CMD ["/bin/bash"] 