# LogAI C++ Preprocessor

The LogAI C++ Preprocessor is a high-performance component for preprocessing log data before it's parsed by the DRAIN log parser or other log parsers. The preprocessor is designed to be highly efficient and memory conscious, ideal for working with large log files.

## Features

- **High-performance**: Optimized C++ implementation with parallel processing for large log batches
- **Memory-efficient**: Careful memory management to handle large log files
- **Customizable**: Configurable regex patterns for delimiters and replacements
- **Attribute extraction**: Ability to extract and store structured log attributes
- **Timestamp identification**: Automatic detection of various timestamp formats
- **Integration with FileDataLoader**: Seamlessly integrated with the main LogAI data loading pipeline

## Usage

### Basic Usage

The preprocessor can be used directly or through the FileDataLoader integration:

```cpp
// Configure the preprocessor
PreprocessorConfig config(
    {
        // Delimiter regex patterns 
        { R"(\[)", " [ " },
        { R"(\])", " ] " }
    },
    {
        // Replacement patterns
        { R"(\b\d+\.\d+\.\d+\.\d+\b)", "<IP_ADDRESS>" },
        { R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})", "<UUID>" }
    }
);

// Create and use the preprocessor
Preprocessor preprocessor(config);
auto [cleaned_logs, extracted_terms] = preprocessor.clean_log_batch(log_lines);
```

### Integration with FileDataLoader

The preprocessor is integrated with the FileDataLoader for seamless usage:

```cpp
// Configure data loader with preprocessor
DataLoaderConfig config;
config.file_path = log_file_path;
config.enable_preprocessing = true;
config.custom_delimiters_regex = {
    { R"(\[)", " [ " },
    { R"(\])", " ] " }
};
config.custom_replace_list = {
    { R"(\b\d+\.\d+\.\d+\.\d+\b)", "<IP_ADDRESS>" },
    { R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})", "<UUID>" }
};

FileDataLoader data_loader(config);

// The preprocessor will automatically be applied during parsing
std::vector<LogRecordObject> logs = data_loader.load_data();
```

### Attribute Extraction

The preprocessor can extract structured attributes from logs:

```cpp
std::unordered_map<std::string, std::string> patterns = {
    {"severity", R"(\[(INFO|WARN|ERROR|DEBUG)\])"},
    {"timestamp", R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?))"}, 
    {"ip_address", R"((\d+\.\d+\.\d+\.\d+))"}
};

auto attributes_table = data_loader.extract_attributes(log_lines, patterns);
```

## Best Practices

1. **Optimize regex patterns**: Be careful with complex regex patterns that might be computationally expensive. Test with small samples first.

2. **Use the right batch size**: For very large log files, adjust the batch size in the DataLoaderConfig to balance memory usage and performance.

3. **Combine with DRAIN parsing**: The preprocessor works best when combined with the DRAIN log parser, as it can normalize log messages before template extraction.

4. **Store extracted terms**: Consider storing extracted terms for further analysis, especially for fields like IPs, UUIDs, and timestamps.

## Performance Considerations

- The preprocessor automatically uses parallel processing for large batches (>1000 lines)
- Memory usage scales with the number of log lines and the number of replacement patterns
- The regex operations are the most CPU-intensive part of preprocessing; try to minimize their complexity
- Precompiled regex patterns are used for better performance

## Running the Tests

Use the provided test script to run the preprocessor tests:

```bash
./scripts/run-preprocessor-test.sh
```

This script creates a test log file if none exists and runs the preprocessor test application. 