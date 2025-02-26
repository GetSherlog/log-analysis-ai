#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <numeric>
#include "preprocessor.h"
#include "drain_parser.h"
#include "file_data_loader.h"
#include "data_loader_config.h"

using namespace logai;

// Simple performance timing helper
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string label_;
public:
    Timer(const std::string& label) : label_(label) {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    ~Timer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_).count();
        std::cout << label_ << ": " << duration << " ms" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <log_file_path>" << std::endl;
        return 1;
    }

    const std::string log_file_path = argv[1];
    
    try {
        std::cout << "====== PART 1: Testing Direct Preprocessor Usage ======" << std::endl;
        
        // Configure the preprocessor directly
        PreprocessorConfig preprocessor_config(
            {
                // Example delimiter regex patterns
                { R"(\[)", " [ " },
                { R"(\])", " ] " },
                { R"(\()", " ( " },
                { R"(\))", " ) " }
            },
            {
                // Example replacement patterns
                { R"(\b\d+\.\d+\.\d+\.\d+\b)", "<IP_ADDRESS>" },
                { R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})", "<UUID>" },
                { R"(\b\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})?\b)", "<TIMESTAMP>" }
            }
        );
        
        Preprocessor preprocessor(preprocessor_config);
        
        // Configure the DRAIN parser
        DataLoaderConfig loader_config;
        loader_config.file_path = log_file_path;
        DrainParser drain_parser(loader_config, 4, 0.5, 100);
        
        // Read test log file
        std::ifstream file(log_file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open log file: " << log_file_path << std::endl;
            return 1;
        }
        
        std::vector<std::string> log_lines;
        std::string line;
        
        // Read first 1000 lines for testing (to keep it quick)
        size_t max_lines = 1000;
        size_t line_count = 0;
        
        while (std::getline(file, line) && line_count < max_lines) {
            log_lines.push_back(line);
            line_count++;
        }
        
        std::cout << "Read " << log_lines.size() << " log lines" << std::endl;
        
        // Preprocess log lines directly
        {
            Timer timer("Direct preprocessing time");
            auto [cleaned_logs, extracted_terms] = preprocessor.clean_log_batch(log_lines);
            
            // Print some stats about extracted terms
            for (const auto& [key, values] : extracted_terms) {
                size_t term_count = std::accumulate(values.begin(), values.end(), 0,
                    [](size_t sum, const std::vector<std::string>& terms) {
                        return sum + terms.size();
                    });
                std::cout << "Extracted " << term_count << " instances of " << key << std::endl;
            }
            
            // Parse a few cleaned logs with DRAIN
            const size_t sample_size = std::min(static_cast<size_t>(5), cleaned_logs.size());
            std::cout << "\nSample cleaned logs:" << std::endl;
            for (size_t i = 0; i < sample_size; ++i) {
                std::cout << i+1 << ": " << cleaned_logs[i] << std::endl;
            }
        }
        
        std::cout << "\n====== PART 2: Testing Integrated Preprocessor in FileDataLoader ======" << std::endl;
        
        // Configure data loader with preprocessor
        DataLoaderConfig integrated_config;
        integrated_config.file_path = log_file_path;
        integrated_config.enable_preprocessing = true;
        integrated_config.custom_delimiters_regex = {
            { R"(\[)", " [ " },
            { R"(\])", " ] " },
            { R"(\()", " ( " },
            { R"(\))", " ) " }
        };
        integrated_config.custom_replace_list = {
            { R"(\b\d+\.\d+\.\d+\.\d+\b)", "<IP_ADDRESS>" },
            { R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})", "<UUID>" },
            { R"(\b\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})?\b)", "<TIMESTAMP>" }
        };
        
        FileDataLoader data_loader(integrated_config);
        
        // Test preprocessing with the FileDataLoader
        {
            Timer timer("Integrated preprocessing time");
            auto preprocessed_logs = data_loader.preprocess_logs(log_lines);
            
            // Print sample preprocessed logs
            const size_t sample_size = std::min(static_cast<size_t>(5), preprocessed_logs.size());
            std::cout << "\nSample preprocessed logs via FileDataLoader:" << std::endl;
            for (size_t i = 0; i < sample_size; ++i) {
                std::cout << i+1 << ": " << preprocessed_logs[i] << std::endl;
            }
        }
        
        // Test attribute extraction
        {
            Timer timer("Attribute extraction time");
            std::unordered_map<std::string, std::string> patterns = {
                {"severity", R"(\[(INFO|WARN|ERROR|DEBUG)\])"}, 
                {"timestamp", R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?))"}, 
                {"ip_address", R"((\d+\.\d+\.\d+\.\d+))"}
            };
            
            auto attributes_table = data_loader.extract_attributes(log_lines, patterns);
            
            if (attributes_table) {
                std::cout << "\nExtracted attributes table schema:" << std::endl;
                std::cout << attributes_table->schema()->ToString() << std::endl;
                std::cout << "Table has " << attributes_table->num_rows() << " rows and " 
                          << attributes_table->num_columns() << " columns" << std::endl;
            }
        }
        
        std::cout << "\nPreprocessor test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 