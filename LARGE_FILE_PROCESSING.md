# LogAI-CPP Large File Processing

This document provides instructions and best practices for using the LogAI-CPP library to process very large log files efficiently, without hitting memory limitations.

## Overview

The LogAI-CPP library now includes enhanced capabilities for processing large log files through automatic chunking, memory-adaptive processing, and efficient resource management. These improvements allow processing multi-gigabyte log files without hitting memory limits that would otherwise cause the application to abort.

## Key Features

1. **Automatic Chunking**: Automatically splits large files into manageable chunks that can be processed independently.
2. **Memory-Adaptive Processing**: Dynamically adjusts batch sizes based on current memory usage and processing queue depth.
3. **Gradual Processing**: Controls the rate of log line processing to prevent memory overloads.
4. **Parallel Processing**: Efficiently utilizes multiple CPU cores while managing memory consumption.
5. **Backpressure Handling**: Implements backpressure mechanisms to slow down producers when consumers can't keep up.

## Usage

### Using the Enhanced Performance Test Script

The simplest way to utilize these improvements is with the provided `run-large-perf-test.sh` script:

```bash
./scripts/run-large-perf-test.sh --parser drain --input your_large_logfile.log --memory 4g
```

Options:
- `--parser TYPE` - Parser type (csv, json, drain, regex) [default: drain]
- `--input FILE` - Input log file path (uses default HDFS logs if not specified)
- `--output FILE` - Output Parquet file path [default: ./test_data/output.parquet]
- `--memory LIMIT` - Memory limit for Docker container (e.g., 3g, 4g) [default: 3g]
- `--chunk-size SIZE` - Size of chunks in bytes (0 = auto) [default: 0]

### Using in Your Own Code

To use the large file processing capabilities in your own code:

```cpp
#include "src/file_data_loader.h"
#include "src/data_loader_config.h"

using namespace logai;

// Configure the data loader
DataLoaderConfig config;
config.file_path = "path/to/large/logfile.log";
config.log_type = "drain";  // or "csv", "json", "regex"
config.num_threads = 10;
config.use_memory_mapping = true;

// Create the loader
FileDataLoader loader(config);

// Process the large file with automatic chunking
// The memory_limit parameter is in bytes (2GB in this example)
auto table = loader.process_large_file("output.parquet", 2UL * 1024 * 1024 * 1024);
```

## How It Works

1. **File Size Analysis**: The system analyzes the file size and estimates an optimal chunk size.
2. **Sample-Based Estimation**: A small sample of the file is read to estimate the average log line size.
3. **Chunking Strategy**: Based on these estimates, the file is split into manageable chunks.
4. **Per-Chunk Processing**: Each chunk is processed independently, with results merged at the end.
5. **Memory Monitoring**: During processing, memory usage is monitored, and batch sizes are adjusted accordingly.
6. **Automatic Rate Control**: Processing speed is automatically controlled based on available resources.

## Performance Considerations

- **Memory Allocation**: Set the memory limit based on your system's available RAM. A good rule of thumb is to use 70-80% of available physical memory.
- **Thread Count**: The default thread count is set to the number of available CPU cores. Adjust this in the configuration if needed.
- **Temporary Storage**: Ensure sufficient disk space for temporary files during processing.
- **Parser Selection**: For very large files, the drain parser generally offers the best performance and memory efficiency.

## Troubleshooting

If you encounter issues:

1. **Reduce Memory Pressure**: Lower the memory limit or increase the chunk size to reduce memory pressure.
2. **Check Disk Space**: Ensure sufficient disk space for temporary files and the output Parquet file.
3. **Examine Logs**: Check the console output for any warnings or errors that might indicate problems.
4. **Reduce Thread Count**: If memory pressure persists, try reducing the number of worker threads.

## Limitations

- The chunking approach may result in slightly different pattern extraction when using the drain parser compared to processing the entire file at once.
- For extremely large files (tens of GB), processing times can still be substantial.
- External memory pressure from other applications may affect processing performance.

## Further Improvements

Future improvements to the large file processing capabilities may include:

- Implementation of streaming Parquet writing to reduce memory requirements further
- More sophisticated memory usage prediction and adaptation
- Support for distributed processing across multiple nodes
- Checkpointing for resumable processing after interruptions 