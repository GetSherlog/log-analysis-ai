#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include "preprocessor.h"
#include "simd_string_ops.h"

using namespace logai;

// Function to measure execution time
template<typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// Generate synthetic log lines
std::vector<std::string> generate_logs(size_t count, size_t avg_line_length) {
    std::vector<std::string> logs;
    logs.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(avg_line_length/2, avg_line_length*3/2);
    std::uniform_int_distribution<> char_dist(32, 126);  // ASCII printable characters
    
    // Add some common log patterns
    std::vector<std::string> templates = {
        "INFO [%timestamp%] User %userid% logged in from %ip%",
        "ERROR [%timestamp%] Failed to connect to database: %error%",
        "WARN [%timestamp%] High memory usage: %memory%MB",
        "DEBUG [%timestamp%] Processing request %requestid% with params: %params%",
        "INFO [%timestamp%] Request completed in %time%ms"
    };
    
    for (size_t i = 0; i < count; ++i) {
        std::ostringstream oss;
        
        // Occasionally use a template
        if (i % 3 == 0 && !templates.empty()) {
            size_t template_idx = i % templates.size();
            oss << templates[template_idx];
            
            // Replace placeholders with random data
            std::string& log = oss.str();
            size_t pos = 0;
            while ((pos = log.find('%', pos)) != std::string::npos) {
                size_t end_pos = log.find('%', pos + 1);
                if (end_pos == std::string::npos) break;
                
                // Generate random data for the placeholder
                std::string placeholder = log.substr(pos, end_pos - pos + 1);
                std::string replacement;
                
                if (placeholder == "%timestamp%") {
                    replacement = "2023-08-15T14:32:" + std::to_string(i % 60) + "." + std::to_string(i % 1000);
                } else if (placeholder == "%userid%") {
                    replacement = "user" + std::to_string(i % 1000);
                } else if (placeholder == "%ip%") {
                    replacement = "192.168." + std::to_string(i % 256) + "." + std::to_string(i % 256);
                } else if (placeholder == "%error%") {
                    replacement = "Connection timed out after 30s";
                } else if (placeholder == "%memory%") {
                    replacement = std::to_string(1000 + (i % 7000));
                } else if (placeholder == "%requestid%") {
                    replacement = "REQ-" + std::to_string(i);
                } else if (placeholder == "%params%") {
                    replacement = "{\"id\":" + std::to_string(i) + ",\"action\":\"get\"}";
                } else if (placeholder == "%time%") {
                    replacement = std::to_string(10 + (i % 990));
                } else {
                    replacement = "value" + std::to_string(i);
                }
                
                log.replace(pos, end_pos - pos + 1, replacement);
                pos += replacement.length();
            }
            
            logs.push_back(log);
        } else {
            // Generate a random log line
            size_t line_length = len_dist(gen);
            for (size_t j = 0; j < line_length; ++j) {
                oss << static_cast<char>(char_dist(gen));
            }
            logs.push_back(oss.str());
        }
    }
    
    return logs;
}

// Compare regular and SIMD preprocessor performance
void benchmark_preprocessor() {
    std::cout << "Generating synthetic log data..." << std::endl;
    std::vector<std::string> logs = generate_logs(100000, 100);
    
    std::cout << "Log data generated: " << logs.size() << " lines" << std::endl;
    
    // Create preprocessor configurations
    PreprocessorConfig config_regular(
        {{"\\s+", " "}},  // Simple space normalization
        {{"(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2})", "<TIMESTAMP>"}},
        false  // Don't use SIMD
    );
    
    PreprocessorConfig config_simd(
        {{"\\s+", " "}},  // Simple space normalization
        {{"(\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2})", "<TIMESTAMP>"}},
        true   // Use SIMD
    );
    
    Preprocessor preprocessor_regular(config_regular);
    Preprocessor preprocessor_simd(config_simd);
    
    // Measure normal preprocessor performance
    std::cout << "Testing regular preprocessor..." << std::endl;
    double regular_time = measure_time([&]() {
        auto [cleaned_logs, terms] = preprocessor_regular.clean_log_batch(logs);
    });
    
    // Measure SIMD preprocessor performance
    std::cout << "Testing SIMD preprocessor..." << std::endl;
    double simd_time = measure_time([&]() {
        auto [cleaned_logs, terms] = preprocessor_simd.clean_log_batch(logs);
    });
    
    std::cout << "\nResults:" << std::endl;
    std::cout << "Regular preprocessing: " << regular_time << "ms" << std::endl;
    std::cout << "SIMD preprocessing:    " << simd_time << "ms" << std::endl;
    std::cout << "Speedup factor:        " << (regular_time / simd_time) << "x" << std::endl;
}

// Test individual SIMD string operations
void test_simd_string_ops() {
    std::cout << "\nTesting SIMD string operations..." << std::endl;
    
    // Test strings of different sizes
    std::vector<size_t> sizes = {100, 1000, 10000, 100000};
    
    for (size_t size : sizes) {
        std::string test_string(size, 'a');
        
        // Add some delimiters to replace
        for (size_t i = 0; i < size; i += 10) {
            test_string[i] = ',';
        }
        
        std::cout << "\nString size: " << size << " bytes" << std::endl;
        
        // Test replace_char
        double std_time = measure_time([&]() {
            std::string result = test_string;
            for (size_t i = 0; i < result.size(); ++i) {
                if (result[i] == ',') {
                    result[i] = ' ';
                }
            }
        });
        
        double simd_time = measure_time([&]() {
            std::string result = SimdStringOps::replace_char(test_string, ',', ' ');
        });
        
        std::cout << "replace_char:    std=" << std_time << "ms, simd=" << simd_time 
                  << "ms, speedup=" << (std_time/simd_time) << "x" << std::endl;
        
        // Test to_lower
        std_time = measure_time([&]() {
            std::string result = test_string;
            for (size_t i = 0; i < result.size(); ++i) {
                result[i] = std::tolower(result[i]);
            }
        });
        
        simd_time = measure_time([&]() {
            std::string result = SimdStringOps::to_lower(test_string);
        });
        
        std::cout << "to_lower:        std=" << std_time << "ms, simd=" << simd_time 
                  << "ms, speedup=" << (std_time/simd_time) << "x" << std::endl;
    }
}

// The main function
int main(int argc, char** argv) {
    std::cout << "===== SIMD Preprocessing Performance Test =====" << std::endl;
    
    // Test basic SIMD operations
    test_simd_string_ops();
    
    // Test full preprocessor
    benchmark_preprocessor();
    
    return 0;
} 