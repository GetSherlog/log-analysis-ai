# LogAI-CPP

A high-performance log analysis library written in C++.

## Features

- Fast log file loading from various formats (CSV, JSON, custom regex patterns)
- Parallel log processing with multi-threading
- Memory-mapped file reading for large log files
- Zero-copy data integration with Apache Arrow
- SIMD-accelerated parsing operations
- Support for structured log formats and custom patterns
- High-performance DRAIN log parser for efficient log template extraction

## Log Parsers

The library includes several log parsers for different formats:

- **CSV Parser**: For parsing CSV-formatted logs
- **JSON Parser**: For parsing JSON-formatted logs
- **Regex Parser**: For parsing logs using custom regex patterns
- **DRAIN Parser**: A high-performance implementation of the DRAIN log parsing algorithm, which efficiently groups similar log messages and extracts their templates and parameters

## Apache Arrow Integration

The library uses Apache Arrow as its dataframe implementation, which provides several benefits:

- **High Performance**: Arrow's columnar memory layout enables vectorized operations
- **Zero-Copy Sharing**: Share data between different tools without serialization/deserialization
- **Language Interoperability**: Easily work with the same data in Python, R, and other languages
- **Ecosystem Integration**: Smooth integration with tools like DuckDB, Parquet, and ML frameworks
- **Memory Efficiency**: Process larger-than-memory datasets with streaming capabilities

## Installation

### Prerequisites

- C++17 compatible compiler
- CMake 3.10 or higher
- Apache Arrow C++ libraries

### Building From Source

```bash
# Install Apache Arrow (varies by platform)
# For macOS:
brew install apache-arrow

# For Ubuntu:
# apt install libarrow-dev

# Clone the repository
git clone https://github.com/yourusername/logai-cpp.git
cd logai-cpp

# Create build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build
make

# Install
make install
```

## Usage

Here's a basic example of reading logs and converting them to an Arrow dataframe:

```cpp
#include <logai/dataloader/file_data_loader.h>
#include <logai/dataloader/data_loader_config.h>
#include <arrow/api.h>

int main() {
    // Configure the data loader
    logai::DataLoaderConfig config;
    config.file_path = "example_logs.json";
    config.log_type = "json";
    config.num_threads = 4;
    config.dimensions = {"timestamp", "severity", "body", "service_name"};
    
    // Create the data loader
    logai::FileDataLoader loader(config);
    
    // Load logs and convert to Arrow dataframe
    std::shared_ptr<arrow::Table> df = loader.log_to_dataframe("example_logs.json", "json");
    
    // Work with the Arrow Table
    std::cout << "Number of rows: " << df->num_rows() << std::endl;
    std::cout << "Number of columns: " << df->num_columns() << std::endl;
    
    return 0;
}
```

## License

This project is licensed under the BSD-3-Clause License - see the LICENSE file for details.

## Performance Testing

A performance test is included to benchmark the library's log parsing and Parquet export capabilities:

1. To run the performance test:
   ```bash
   ./run-perf-test.sh
   ```

2. The test will:
   - Parse a sample log file (or you can provide your own)
   - Convert it to Arrow Table format
   - Export it to Parquet
   - Measure and report timing for each operation

3. For detailed performance test documentation, see [PERFORMANCE_TEST.md](PERFORMANCE_TEST.md) 