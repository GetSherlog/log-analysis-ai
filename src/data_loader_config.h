#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <thread>

namespace logai {

struct DataLoaderConfig {
    std::string file_path;
    std::string log_type = "CSV";
    std::vector<std::string> dimensions;
    std::string datetime_format = "%Y-%m-%dT%H:%M:%SZ";
    bool infer_datetime = false;
    std::string log_pattern = ""; // Default regex pattern for log parsing
    
    // Performance configuration
    size_t num_threads = std::thread::hardware_concurrency();
    size_t batch_size = 10000;
    bool use_memory_mapping = true;
    bool use_simd = true;
};
} 