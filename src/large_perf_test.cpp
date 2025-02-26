#include "file_data_loader.h"
#include "data_loader_config.h"
#include "drain_parser.h"
#include "json_parser.h"
#include "csv_parser.h"
#include "regex_parser.h"
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --input FILENAME      Input log file path\n"
              << "  --output FILENAME     Output Parquet file path\n"
              << "  --parser TYPE         Parser type (drain, json, csv, regex)\n"
              << "  --memory-limit MB     Memory limit in MB\n"
              << "  --chunk-size LINES    Initial chunk size (lines per batch)\n"
              << "  --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    // Default values
    std::string input_file = "";
    std::string output_file = "output.parquet";
    std::string parser_type = "drain";
    size_t memory_limit_mb = 3000; // 3GB default
    size_t chunk_size = 10000;     // 10K lines default

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--input" && i + 1 < argc) {
            input_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--parser" && i + 1 < argc) {
            parser_type = argv[++i];
        } else if (arg == "--memory-limit" && i + 1 < argc) {
            memory_limit_mb = std::stoul(argv[++i]);
        } else if (arg == "--chunk-size" && i + 1 < argc) {
            chunk_size = std::stoul(argv[++i]);
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: Input file must be specified with --input\n";
        print_usage(argv[0]);
        return 1;
    }

    try {
        // Check if input file exists
        if (!fs::exists(input_file)) {
            std::cerr << "Error: Input file does not exist: " << input_file << std::endl;
            return 1;
        }

        // Create output directory if it doesn't exist
        fs::path output_path(output_file);
        if (!output_path.parent_path().empty() && !fs::exists(output_path.parent_path())) {
            fs::create_directories(output_path.parent_path());
        }

        // Configure data loader
        logai::DataLoaderConfig config;
        config.file_path = input_file;
        config.log_type = parser_type;
        config.num_threads = 8; // Adjust based on available CPU cores
        config.use_memory_mapping = true;
        
        std::cout << "Processing log file: " << input_file << std::endl;
        std::cout << "Using parser: " << parser_type << std::endl;
        std::cout << "Memory limit: " << memory_limit_mb << " MB" << std::endl;
        std::cout << "Initial chunk size: " << chunk_size << " lines" << std::endl;
        
        // Start performance measurement
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create the data loader
        logai::FileDataLoader loader(config);
        
        // Process large file with automatic chunking
        std::cout << "Starting large file processing..." << std::endl;
        std::cout << "Input file: " << input_file << std::endl;
        std::cout << "Parser type: " << parser_type << std::endl;
        std::cout << "Memory limit: " << memory_limit_mb << " MB" << std::endl;
        std::cout << "Chunk size: " << chunk_size << " lines" << std::endl;

        // Get file size
        size_t file_size = std::filesystem::file_size(input_file) / (1024 * 1024.0);
        std::cout << "File size: " << file_size << " MB" << std::endl;

        // Force chunking for files over 10MB regardless of memory limit
        bool force_chunking = file_size > 10;

        std::shared_ptr<arrow::Table> table;
        if (force_chunking) {
            std::cout << "File is large enough to require chunking." << std::endl;
            table = loader.process_large_file(input_file, parser_type, memory_limit_mb, chunk_size, true);
        } else {
            std::cout << "File is small enough to process directly." << std::endl;
            table = loader.process_large_file(input_file, parser_type, memory_limit_mb, chunk_size, false);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Successfully processed " << table->num_rows() 
                  << " log records in " << duration.count() << " ms" << std::endl;
        
        // Write table to Parquet file
        auto start_write = std::chrono::high_resolution_clock::now();
        
        std::shared_ptr<arrow::io::FileOutputStream> outfile;
        PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(output_file));
        
        // Set up Parquet writer properties
        parquet::WriterProperties::Builder builder;
        builder.compression(parquet::Compression::SNAPPY);
        builder.version(parquet::ParquetVersion::PARQUET_2_0);
        std::shared_ptr<parquet::WriterProperties> props = builder.build();
        
        // Write the table
        PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, 100000, props));
        
        auto end_write = std::chrono::high_resolution_clock::now();
        auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_write - start_write);
        
        std::cout << "Wrote Parquet file in " << write_duration.count() << " ms" << std::endl;
        std::cout << "Total processing time: " << (duration.count() + write_duration.count()) << " ms" << std::endl;
        std::cout << "Output written to: " << output_file << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 