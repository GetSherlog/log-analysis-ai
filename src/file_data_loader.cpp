/**
 * Copyright (c) 2023 LogAI Team
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include "file_data_loader.h"
#include "csv_parser.h"
#include "json_parser.h"
#include "regex_parser.h"
#include "drain_parser.h"
#include "simd_scanner.h"
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/compute/api.h>
#include <arrow/compute/api_scalar.h>
#include <arrow/compute/api_vector.h>
#include <arrow/compute/cast.h>
#include <arrow/compute/exec.h>
#include <arrow/compute/expression.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <fcntl.h>      // For open, O_RDONLY
#include <sys/mman.h>   // For mmap, munmap, PROT_READ, MAP_PRIVATE, MAP_FAILED
#include <sys/stat.h>   // For stat
#include <unistd.h>     // For close

// Define namespace aliases to avoid conflicts
namespace fs = std::filesystem;

// Bring std types into global scope
using std::string;
using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::thread;
using std::function;
using std::ifstream;
using std::ofstream;
using std::string_view;
using std::cerr;
using std::endl;
using std::runtime_error;
using std::move;
using std::atomic;

namespace logai {

// Constants
constexpr size_t CHUNK_SIZE = 64 * 1024; // 64KB chunks for efficient file reading
constexpr size_t MAX_LINE_LENGTH = 1024 * 1024; // 1MB max line length
constexpr char LOGLINE_NAME[] = "log_message";
constexpr char SPAN_ID[] = "span_id";
constexpr char LOG_TIMESTAMPS[] = "timestamp";
constexpr char LABELS[] = "labels";

FileDataLoader::FileDataLoader(const DataLoaderConfig& config)
    : config_(config) {
}

FileDataLoader::~FileDataLoader() {
    // Empty destructor
}

std::vector<LogRecordObject> FileDataLoader::load_data() {
    std::string filepath = config_.file_path;
    
    // Check if file exists
    if (!fs::exists(filepath)) {
        throw std::runtime_error("File does not exist: " + filepath);
    }
    
    std::vector<LogRecordObject> results;
    running_ = true;
    std::atomic<size_t> total_batches_{0};
    
    // Create queues for the producer-consumer pattern
    ThreadSafeQueue<LogBatch> input_queue;
    ThreadSafeQueue<ProcessedBatch> output_queue;
    
    // Start the producer thread to read the file
    MemoryMappedFile file;
    std::thread producer([this, &file, &input_queue, &total_batches_]() {
        producer_thread(file, input_queue, total_batches_);
    });
    
    // Determine number of worker threads
    size_t num_threads = config_.num_threads > 0 ? 
                        config_.num_threads : 
                        std::thread::hardware_concurrency();
    
    // Start worker threads to process batches
    std::vector<std::thread> workers;
    for (size_t i = 0; i < num_threads; i++) {
        workers.push_back(std::thread([this, &input_queue, &output_queue]() {
            LogBatch batch;
            while (input_queue.wait_and_pop(batch)) {
                ProcessedBatch processed;
                process_batch(batch, processed);
                output_queue.push(std::move(processed));
            }
        }));
    }
    
    // Start consumer thread to collect results
    std::thread consumer([this, num_threads, &output_queue, &results, &total_batches_]() {
        consumer_thread(num_threads, output_queue, results, total_batches_);
    });
    
    // Wait for all threads to complete
    producer.join();
    for (auto& worker : workers) {
        worker.join();
    }
    
    // Signal that no more processed batches will be produced
    output_queue.done();
    consumer.join();
    
    running_ = false;
    return results;
}

std::unique_ptr<LogParser> FileDataLoader::create_parser() {
    if (config_.log_type == "csv") {
        return std::make_unique<CsvParser>(config_);
    } else if (config_.log_type == "json") {
        return std::make_unique<JsonParser>(config_);
    } else if (config_.log_type == "drain") {
        // Use the high-performance DRAIN parser for log pattern parsing
        return std::make_unique<DrainParser>(config_);
    } else {
        // Default to regex parser for custom log formats
        return std::make_unique<RegexParser>(config_, config_.log_pattern);
    }
}

void FileDataLoader::process_batch(const LogBatch& batch, ProcessedBatch& result) {
    result.batch_id = batch.batch_id;
    result.records.reserve(batch.lines.size());
    
    auto parser = create_parser();
    
    for (const auto& line : batch.lines) {
        try {
            if (line.empty()) continue;
            
            if (config_.log_type == "csv" || config_.log_type == "tsv") {
                result.records.push_back(parser->parse_line(line));
            } else if (config_.log_type == "json") {
                result.records.push_back(parser->parse_line(line));
            } else {
                // Custom log format
                result.records.push_back(parser->parse_line(line));
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing line: " << e.what() << std::endl;
        }
    }
}

void FileDataLoader::producer_thread(MemoryMappedFile& file, ThreadSafeQueue<LogBatch>& input_queue, 
                                    std::atomic<size_t>& total_batches) {
    try {
        if (config_.use_memory_mapping) {
            read_file_memory_mapped(config_.file_path, [&](std::string_view line) {
                LogBatch batch{total_batches++, {std::string(line)}};
                input_queue.push(std::move(batch));
            });
        } else {
            read_file_by_chunks(config_.file_path, [&](const std::string& line) {
                LogBatch batch{total_batches++, {line}};
                input_queue.push(std::move(batch));
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in producer thread: " << e.what() << std::endl;
    }
    
    // Signal that no more batches will be produced
    input_queue.done();
}

void FileDataLoader::consumer_thread(size_t num_threads, ThreadSafeQueue<ProcessedBatch>& output_queue, 
                                    std::vector<LogRecordObject>& results,
                                    std::atomic<size_t>& total_batches) {
    size_t processed_count = 0;
    size_t expected_count = total_batches.load();
    
    while (true) {
        ProcessedBatch batch;
        if (!output_queue.wait_and_pop(batch)) {
            break; // Queue is done and empty
        }
        
        // Add the processed records to the results
        results.insert(results.end(), 
                      std::make_move_iterator(batch.records.begin()), 
                      std::make_move_iterator(batch.records.end()));
        
        processed_count++;
        
        // If we've processed all expected batches, we're done
        if (processed_count >= expected_count && expected_count > 0) {
            break;
        }
    }
}

void FileDataLoader::read_file_by_chunks(const std::string& filepath, 
                                       const std::function<void(const std::string&)>& callback) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            callback(line);
        }
    }
}

void FileDataLoader::read_file_memory_mapped(const std::string& filepath,
                                           const std::function<void(std::string_view)>& callback) {
    MemoryMappedFile file;
    if (!file.open(filepath)) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    auto scanner = file.getScanner();
    if (!scanner) {
        throw std::runtime_error("Failed to create scanner for file: " + filepath);
    }
    
    while (!scanner->atEnd()) {
        size_t newline_pos = scanner->findNewline();
        std::string_view line = scanner->getSubstring(newline_pos - scanner->position());
        scanner->advance(newline_pos + 1);
        
        if (!line.empty()) {
            callback(line);
        }
    }
}

std::vector<LogRecordObject> FileDataLoader::read_logs(const std::string& filepath) {
    std::vector<LogRecordObject> records;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    auto parser = create_parser();
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        try {
            records.push_back(parser->parse_line(line));
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << e.what() << std::endl;
        }
    }
    
    return records;
}

std::vector<LogRecordObject> FileDataLoader::read_csv(const std::string& filepath) {
    return read_logs(filepath);
}

std::vector<LogRecordObject> FileDataLoader::read_tsv(const std::string& filepath) {
    return read_logs(filepath);
}

std::vector<LogRecordObject> FileDataLoader::read_json(const std::string& filepath) {
    return read_logs(filepath);
}

std::shared_ptr<arrow::Table> FileDataLoader::log_to_dataframe(const std::string& filepath, const std::string& log_format) {
    // Load log records
    std::vector<LogRecordObject> log_records;
    
    if (log_format == "CSV") {
        log_records = read_csv(filepath);
    } else if (log_format == "JSON") {
        log_records = read_json(filepath);
    } else {
        log_records = read_logs(filepath);
    }
    
    // Create Arrow builders
    arrow::StringBuilder body_builder;
    arrow::StringBuilder severity_builder;
    arrow::Int64Builder timestamp_builder;
    
    // Create builders for attributes (dynamically determine them from the data)
    std::unordered_map<std::string, std::shared_ptr<arrow::StringBuilder>> attribute_builders;
    std::unordered_set<std::string> attribute_keys;
    
    // First pass: collect all attribute keys
    for (const auto& record : log_records) {
        for (const auto& [key, value] : record.attributes) {
            attribute_keys.insert(key);
        }
    }
    
    // Initialize attribute builders
    for (const auto& key : attribute_keys) {
        attribute_builders[key] = std::make_shared<arrow::StringBuilder>();
    }
    
    // Second pass: populate the arrays
    for (const auto& record : log_records) {
        // Add body
        auto status = body_builder.Append(record.body);
        if (!status.ok()) {
            throw std::runtime_error("Failed to append body: " + status.ToString());
        }
        
        // Add severity
        if (record.severity) {
            status = severity_builder.Append(*record.severity);
            if (!status.ok()) {
                throw std::runtime_error("Failed to append severity: " + status.ToString());
            }
        } else {
            status = severity_builder.AppendNull();
            if (!status.ok()) {
                throw std::runtime_error("Failed to append null severity: " + status.ToString());
            }
        }
        
        // Add timestamp
        if (record.timestamp) {
            auto timestamp_value = std::chrono::duration_cast<std::chrono::milliseconds>(
                record.timestamp->time_since_epoch()).count();
            status = timestamp_builder.Append(timestamp_value);
            if (!status.ok()) {
                throw std::runtime_error("Failed to append timestamp: " + status.ToString());
            }
        } else {
            status = timestamp_builder.AppendNull();
            if (!status.ok()) {
                throw std::runtime_error("Failed to append null timestamp: " + status.ToString());
            }
        }
        
        // Add attributes
        for (const auto& key : attribute_keys) {
            auto builder = attribute_builders[key];
            auto it = record.attributes.find(key);
            
            if (it != record.attributes.end()) {
                status = builder->Append(it->second);
                if (!status.ok()) {
                    throw std::runtime_error("Failed to append attribute: " + status.ToString());
                }
            } else {
                status = builder->AppendNull();
                if (!status.ok()) {
                    throw std::runtime_error("Failed to append null attribute: " + status.ToString());
                }
            }
        }
    }
    
    // Finalize arrays
    std::shared_ptr<arrow::Array> body_array;
    auto result = body_builder.Finish();
    if (!result.ok()) {
        throw std::runtime_error("Failed to finish body array: " + result.status().ToString());
    }
    body_array = result.ValueOrDie();
    
    std::shared_ptr<arrow::Array> severity_array;
    result = severity_builder.Finish();
    if (!result.ok()) {
        throw std::runtime_error("Failed to finish severity array: " + result.status().ToString());
    }
    severity_array = result.ValueOrDie();
    
    std::shared_ptr<arrow::Array> timestamp_array;
    result = timestamp_builder.Finish();
    if (!result.ok()) {
        throw std::runtime_error("Failed to finish timestamp array: " + result.status().ToString());
    }
    timestamp_array = result.ValueOrDie();
    
    // Create field vector and arrays vector
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    // Add standard fields
    fields.push_back(arrow::field("body", arrow::utf8()));
    arrays.push_back(body_array);
    
    fields.push_back(arrow::field("severity", arrow::utf8()));
    arrays.push_back(severity_array);
    
    fields.push_back(arrow::field("timestamp", arrow::int64()));
    arrays.push_back(timestamp_array);
    
    // Add attribute fields
    for (const auto& key : attribute_keys) {
        fields.push_back(arrow::field(key, arrow::utf8()));
        std::shared_ptr<arrow::Array> array;
        result = attribute_builders[key]->Finish();
        if (!result.ok()) {
            throw std::runtime_error("Failed to finish attribute array: " + result.status().ToString());
        }
        array = result.ValueOrDie();
        arrays.push_back(array);
    }
    
    // Create schema
    auto schema = std::make_shared<arrow::Schema>(fields);
    
    // Create table
    return arrow::Table::Make(schema, arrays);
}

std::shared_ptr<arrow::Table> FileDataLoader::filter_dataframe(std::shared_ptr<arrow::Table> table, 
                                                              const std::string& column, 
                                                              const std::string& op, 
                                                              const std::string& value) {
    try {
        // For now, just return the original table
        // This is a simplified implementation to avoid Arrow API compatibility issues
        std::cout << "Warning: filter_dataframe is not fully implemented in this version." << std::endl;
        return table;
    } catch (const std::exception& e) {
        throw std::runtime_error("Error filtering dataframe: " + std::string(e.what()));
    }
}

arrow::Status FileDataLoader::write_to_parquet(std::shared_ptr<arrow::Table> table, 
                                             const std::string& output_filepath) {
    try {
        // Create output file
        std::shared_ptr<arrow::io::FileOutputStream> outfile;
        ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open(output_filepath));
        
        // Set up Parquet writer properties with default settings
        std::shared_ptr<parquet::WriterProperties> props = parquet::default_writer_properties();
            
        // Set up Arrow-specific Parquet writer properties with default settings
        std::shared_ptr<parquet::ArrowWriterProperties> arrow_props = parquet::default_arrow_writer_properties();
            
        // Write table to Parquet file
        PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), 
                                                     outfile, /*chunk_size=*/65536,
                                                     props, arrow_props));
        
        return arrow::Status::OK();
    } catch (const std::exception& e) {
        return arrow::Status::IOError("Error writing Parquet file: ", e.what());
    }
}

} // namespace logai 