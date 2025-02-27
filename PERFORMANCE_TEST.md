# LogAI-CPP Performance Test

This document explains how to run the performance test for the LogAI-CPP library. The performance test measures the time taken to:

1. Load and parse a log file
2. Convert the log data to an Arrow table
3. Export the data to Parquet format

## Prerequisites

- Docker installed and running
- Bash or compatible shell

## Running the Test

The simplest way to run the performance test is to use the provided script:

```bash
./scripts/run-perf-test.sh
```

This script will:
- Build the Docker image if it doesn't exist
- Create a sample log file with 10,000 lines in `./test_data/sample.log` if it doesn't exist
- Run the performance test in the Docker container
- Report the results, including the size of the output Parquet file

## Parser Options

LogAI-CPP supports multiple log parsers, each with different performance characteristics:

- **CSV Parser**: For structured CSV logs (fastest for well-formatted CSV)
- **JSON Parser**: For JSON-formatted logs
- **Regex Parser**: For custom log formats using regex patterns
- **DRAIN Parser**: A high-performance parser for unstructured logs that automatically extracts templates and parameters

To specify which parser to use, set the `log_type` in the configuration:

```cpp
DataLoaderConfig config;
config.log_type = "drain";  // Use the DRAIN parser
```

The DRAIN parser is particularly useful for unstructured logs where patterns need to be automatically discovered, and it offers better performance than regex-based parsing for complex log formats.

## Customizing the Test

### Using Your Own Log Files

To test with your own log files, place them in the `./test_data` directory before running the test. Then modify the script to use your log file instead of the sample one.

### Modifying Test Parameters

The performance test accepts the following command-line arguments:

- `--input <file>`: Path to the input log file
- `--output <file>`: Path to the output Parquet file
- `--lines <count>`: Number of lines to generate for the sample log (only used if creating a new sample file)

To change these parameters, edit the `run-perf-test.sh` script and modify the command line for the `perf_test` executable.

## Manual Test Execution

If you prefer to run the test manually inside the Docker container, follow these steps:

1. Build the Docker image:
   ```bash
   docker build -t logai-cpp:latest .
   ```

2. Start the Docker container with your working directory mounted:
   ```bash
   docker run -it --rm -v "$(pwd):/app" logai-cpp:latest
   ```

3. Inside the container, build the project:
   ```bash
   mkdir -p build && cd build
   cmake ..
   make -j$(nproc)
   ```

4. Run the performance test:
   ```bash
   ./src/perf_test --input /app/test_data/sample.log --output /app/test_data/output.parquet
   ```

## Performance Metrics

The test will output performance metrics for each stage of the process, including:
- Time taken to parse the log file
- Time taken to convert to Arrow Table
- Time taken to write the Parquet file
- Total execution time

These metrics can be used to compare performance across different environments or to measure the impact of code changes. 