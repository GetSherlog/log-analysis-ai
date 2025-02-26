#include "feature_extractor.h"
#include "file_data_loader.h"
#include "drain_parser.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <arrow/api.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace logai;

// Helper to convert timestamp string to time_point
std::chrono::system_clock::time_point parse_timestamp(const std::string& timestamp_str) {
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Helper to create sample log data
std::vector<LogRecordObject> create_sample_logs() {
    std::vector<LogRecordObject> logs;
    
    // Create 20 sample log records
    for (int i = 0; i < 20; ++i) {
        LogRecordObject log;
        log.body = "Sample log message " + std::to_string(i);
        
        // Add different severity levels
        std::string level = (i % 3 == 0) ? "INFO" : 
                            (i % 3 == 1) ? "WARNING" : "ERROR";
        log.attributes["Level"] = level;
        
        // Add component attribute
        log.attributes["Component"] = "Component-" + std::to_string(i % 2 + 1);
        
        // Add timestamps at 1-second intervals
        auto timestamp = parse_timestamp("2023-01-01T12:00:00");
        timestamp += std::chrono::seconds(i);
        log.timestamp = timestamp;
        
        logs.push_back(log);
    }
    
    return logs;
}

// Helper to create sample feature vectors
std::shared_ptr<arrow::Table> create_sample_feature_vectors(size_t num_logs, size_t vector_size) {
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    for (size_t i = 0; i < vector_size; ++i) {
        fields.push_back(arrow::field("feature_" + std::to_string(i), arrow::float64()));
        
        arrow::DoubleBuilder builder;
        for (size_t j = 0; j < num_logs; ++j) {
            ARROW_EXPECT_OK(builder.Append(static_cast<double>(j + i)));
        }
        
        std::shared_ptr<arrow::Array> array;
        ARROW_EXPECT_OK(builder.Finish(&array));
        arrays.push_back(array);
    }
    
    auto schema = std::make_shared<arrow::Schema>(fields);
    return arrow::Table::Make(schema, arrays);
}

// Test the counter vector functionality
void test_counter_vector() {
    std::cout << "=== Testing counter vector extraction ===" << std::endl;
    
    // Create sample logs
    auto logs = create_sample_logs();
    
    // Configure feature extractor
    FeatureExtractorConfig config;
    config.group_by_category = {"Level"};
    config.group_by_time = "5s";
    
    // Create feature extractor
    FeatureExtractor extractor(config);
    
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();
    
    // Extract counter vectors
    auto result = extractor.convert_to_counter_vector(logs);
    
    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Print results
    std::cout << "Found " << result.group_identifiers.size() << " groups" << std::endl;
    for (size_t i = 0; i < result.group_identifiers.size(); ++i) {
        std::cout << "Group " << i << ":" << std::endl;
        
        // Print group identifiers
        for (const auto& [key, value] : result.group_identifiers[i]) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        
        // Print count
        std::cout << "  Count: " << result.counts[i] << std::endl;
        
        // Print first few indices
        std::cout << "  Indices: ";
        for (size_t j = 0; j < std::min(size_t(5), result.event_indices[i].size()); ++j) {
            std::cout << result.event_indices[i][j] << " ";
        }
        if (result.event_indices[i].size() > 5) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }
    
    std::cout << "Processing time: " << duration.count() << " ms" << std::endl;
}

// Test the feature vector functionality
void test_feature_vector() {
    std::cout << "\n=== Testing feature vector extraction ===" << std::endl;
    
    // Create sample logs and feature vectors
    auto logs = create_sample_logs();
    auto log_vectors = create_sample_feature_vectors(logs.size(), 5);
    
    // Configure feature extractor
    FeatureExtractorConfig config;
    config.group_by_category = {"Level"};
    config.max_feature_len = 10;
    
    // Create feature extractor
    FeatureExtractor extractor(config);
    
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();
    
    // Extract feature vectors
    auto result = extractor.convert_to_feature_vector(logs, log_vectors);
    
    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Print results
    std::cout << "Found " << result.group_identifiers.size() << " groups" << std::endl;
    for (size_t i = 0; i < result.group_identifiers.size(); ++i) {
        std::cout << "Group " << i << ":" << std::endl;
        
        // Print group identifiers
        for (const auto& [key, value] : result.group_identifiers[i]) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        
        // Print first few feature values
        if (result.feature_vectors) {
            std::cout << "  Features: ";
            for (int j = 0; j < std::min(5, result.feature_vectors->num_columns()); ++j) {
                double value;
                if (arrow::GetDoubleValue(result.feature_vectors->column(j), i, &value) == arrow::Status::OK()) {
                    std::cout << value << " ";
                }
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "Processing time: " << duration.count() << " ms" << std::endl;
}

// Test the sequence extraction functionality
void test_sequence_extraction() {
    std::cout << "\n=== Testing sequence extraction ===" << std::endl;
    
    // Create sample logs
    auto logs = create_sample_logs();
    
    // Configure feature extractor with sliding window
    FeatureExtractorConfig config;
    config.group_by_category = {"Component"};
    config.sliding_window = 3;
    config.steps = 1;
    
    // Create feature extractor
    FeatureExtractor extractor(config);
    
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();
    
    // Extract sequences
    auto result = extractor.convert_to_sequence(logs);
    
    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Print results
    std::cout << "Found " << result.sequences.size() << " sequences" << std::endl;
    for (size_t i = 0; i < std::min(size_t(5), result.sequences.size()); ++i) {
        std::cout << "Sequence " << i << ":" << std::endl;
        
        // Print group identifiers
        for (const auto& [key, value] : result.group_identifiers[i]) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        
        // Print sequence
        std::cout << "  Sequence: " << result.sequences[i].substr(0, 50);
        if (result.sequences[i].length() > 50) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }
    
    std::cout << "Processing time: " << duration.count() << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        test_counter_vector();
        test_feature_vector();
        test_sequence_extraction();
        
        std::cout << "\nAll tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 