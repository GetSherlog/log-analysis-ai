/**
 * @file logbert_test.cpp
 * @brief Test program for the LogBERT vectorizer
 */

#include "logbert_vectorizer.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <fstream>

using namespace logai;

// Function to load log file
std::vector<std::string> load_log_file(const std::string& file_path) {
    std::vector<std::string> log_lines;
    std::ifstream file(file_path);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return log_lines;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            log_lines.push_back(line);
        }
    }
    
    file.close();
    return log_lines;
}

// Function to print tokens
void print_tokens(const std::vector<int>& tokens, int max_display = 10) {
    std::cout << "[";
    for (size_t i = 0; i < std::min(max_display, static_cast<int>(tokens.size())); ++i) {
        std::cout << tokens[i];
        if (i < std::min(max_display, static_cast<int>(tokens.size())) - 1) {
            std::cout << ", ";
        }
    }
    
    if (tokens.size() > max_display) {
        std::cout << ", ... (" << tokens.size() - max_display << " more)";
    }
    
    std::cout << "]" << std::endl;
}

// Function to print tokens and attention mask
void print_tokens_with_attention(const std::pair<std::vector<int>, std::vector<int>>& tokens_and_mask, int max_display = 10) {
    const auto& tokens = tokens_and_mask.first;
    const auto& attention_mask = tokens_and_mask.second;
    
    std::cout << "Tokens: ";
    print_tokens(tokens, max_display);
    
    std::cout << "Attn Mask: [";
    for (size_t i = 0; i < std::min(max_display, static_cast<int>(attention_mask.size())); ++i) {
        std::cout << attention_mask[i];
        if (i < std::min(max_display, static_cast<int>(attention_mask.size())) - 1) {
            std::cout << ", ";
        }
    }
    
    if (attention_mask.size() > max_display) {
        std::cout << ", ... (" << attention_mask.size() - max_display << " more)";
    }
    
    std::cout << "]" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <log_file_path> [model_path]" << std::endl;
        std::cerr << "  log_file_path: Path to the log file for processing" << std::endl;
        std::cerr << "  model_path: Optional path to save/load tokenizer model" << std::endl;
        return 1;
    }
    
    std::string log_file_path = argv[1];
    std::string model_path = (argc > 2) ? argv[2] : "./tokenizer_model.json";
    
    std::cout << "LogBERT Vectorizer Test" << std::endl;
    std::cout << "=======================" << std::endl;
    
    // Configure the LogBERT vectorizer
    LogBERTVectorizerConfig config;
    config.model_name = "bert-base-uncased";
    config.max_token_len = 384;
    config.max_vocab_size = 5000;
    config.custom_tokens = {"<IP>", "<TIME>", "<PATH>", "<HEX>"};
    config.num_proc = 8;  // Use 8 threads for processing
    config.output_dir = "./test_data";  // Save tokenizer in test_data directory
    
    // Create vectorizer
    LogBERTVectorizer vectorizer(config);
    
    // Load logs
    std::cout << "Loading log file: " << log_file_path << std::endl;
    auto log_entries = load_log_file(log_file_path);
    std::cout << "Loaded " << log_entries.size() << " log entries" << std::endl;
    
    if (log_entries.empty()) {
        std::cerr << "No log entries loaded. Exiting." << std::endl;
        return 1;
    }
    
    // Try to load an existing tokenizer model
    bool loaded = false;
    try {
        if (vectorizer.is_trained()) {
            std::cout << "Using existing tokenizer" << std::endl;
            loaded = true;
        } else {
            std::cout << "Attempting to load tokenizer from: " << model_path << std::endl;
            loaded = vectorizer.load_tokenizer(model_path);
        }
    } catch (...) {
        // Failed to load, will train new model
        loaded = false;
    }
    
    // Train tokenizer if not loaded
    if (!loaded) {
        std::cout << "Training new tokenizer model" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        vectorizer.fit(log_entries);
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Training completed in " << elapsed.count() << " seconds" << std::endl;
        
        // Save the trained tokenizer
        std::cout << "Saving tokenizer to: " << model_path << std::endl;
        if (vectorizer.save_tokenizer(model_path)) {
            std::cout << "Tokenizer saved successfully" << std::endl;
        } else {
            std::cerr << "Failed to save tokenizer" << std::endl;
        }
    }
    
    // Vectorize logs using legacy method (without attention masks)
    std::cout << "Vectorizing log entries without attention masks..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    auto tokenized_logs = vectorizer.transform(log_entries);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    
    // Print performance stats
    double logs_per_second = log_entries.size() / elapsed.count();
    std::cout << "Vectorization completed in " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << logs_per_second << " logs/second" << std::endl;
    
    // Also test the new method with attention masks
    std::cout << "\nVectorizing log entries with attention masks..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    auto tokenized_logs_with_attention = vectorizer.transform_with_attention(log_entries);
    
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    
    // Print performance stats for the new method
    logs_per_second = log_entries.size() / elapsed.count();
    std::cout << "Vectorization with attention masks completed in " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << logs_per_second << " logs/second" << std::endl;
    
    // Print sample tokenized logs
    std::cout << "\nSample Log Entries and Tokens:" << std::endl;
    std::cout << "=============================" << std::endl;
    
    const int num_samples = std::min(5, static_cast<int>(log_entries.size()));
    for (int i = 0; i < num_samples; ++i) {
        std::cout << "Log[" << i << "]: " << log_entries[i] << std::endl;
        
        std::cout << "Tokens (legacy method)[" << i << "]: ";
        print_tokens(tokenized_logs[i]);
        
        std::cout << "Tokens with attention[" << i << "]: ";
        print_tokens_with_attention(tokenized_logs_with_attention[i]);
        
        std::cout << std::endl;
    }
    
    // Example of using the results with a BERT model
    std::cout << "\nNext steps: Use these token IDs with a BERT model" << std::endl;
    std::cout << "For example, with tokenized_logs_with_attention, you would:" << std::endl;
    std::cout << "1. Extract token_ids and attention_masks arrays" << std::endl;
    std::cout << "2. Pass them to a BERT model API" << std::endl;
    std::cout << "3. Get embeddings or predictions from the model" << std::endl;
    
    return 0;
} 