/**
 * @file logbert_vectorizer.cpp
 * @brief Implementation of LogBERT vectorizer for log data
 */

#include "logbert_vectorizer.h"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <regex>
#include <iostream>
#include <sstream>
#include <future>
#include <thread>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace logai {

//==============================================================================
// WordPieceTokenizer Implementation
//==============================================================================

WordPieceTokenizer::WordPieceTokenizer(const std::string& vocab_file, 
                                     int max_vocab_size,
                                     const std::vector<std::string>& custom_tokens)
    : max_vocab_size_(max_vocab_size) {
    
    // Initialize special tokens
    special_tokens_ = {"[PAD]", "[UNK]", "[CLS]", "[SEP]", "[MASK]"};
    
    // Add special tokens to vocabulary
    int token_id = 0;
    for (const auto& token : special_tokens_) {
        token_to_id_[token] = token_id;
        id_to_token_[token_id] = token;
        token_id++;
    }
    
    // Add custom tokens if provided
    for (const auto& token : custom_tokens) {
        if (token_to_id_.find(token) == token_to_id_.end()) {
            token_to_id_[token] = token_id;
            id_to_token_[token_id] = token;
            token_id++;
        }
    }
    
    // Load vocabulary if file is provided
    if (!vocab_file.empty()) {
        load(vocab_file);
    }
}

void WordPieceTokenizer::train(const std::vector<std::string>& corpus, 
                              int batch_size, 
                              int num_threads) {
    if (corpus.empty()) {
        std::cerr << "Empty corpus provided for training" << std::endl;
        return;
    }
    
    std::cout << "Training WordPiece tokenizer on " << corpus.size() << " documents" << std::endl;
    
    // Count word frequencies
    std::unordered_map<std::string, int> word_counts;
    std::mutex word_counts_mutex;
    
    // Process corpus in batches using multiple threads
    std::vector<std::thread> threads;
    int num_batches = (corpus.size() + batch_size - 1) / batch_size;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int batch = t; batch < num_batches; batch += num_threads) {
                int start = batch * batch_size;
                int end = std::min(start + batch_size, static_cast<int>(corpus.size()));
                
                std::unordered_map<std::string, int> local_word_counts;
                
                for (int i = start; i < end; ++i) {
                    // Determine if we should use uncased normalization (default to true)
                    bool is_uncased = true;
                    std::string normalized = normalize(corpus[i], is_uncased);
                    auto words = pre_tokenize(normalized);
                    
                    for (const auto& word : words) {
                        local_word_counts[word]++;
                    }
                }
                
                // Merge local counts into global counts
                {
                    std::lock_guard<std::mutex> lock(word_counts_mutex);
                    for (const auto& [word, count] : local_word_counts) {
                        word_counts[word] += count;
                    }
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Sort words by frequency
    std::vector<std::pair<std::string, int>> word_freq;
    for (const auto& [word, count] : word_counts) {
        word_freq.emplace_back(word, count);
    }
    
    std::sort(word_freq.begin(), word_freq.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Generate vocabulary based on frequency
    int next_id = token_to_id_.size();
    int max_new_tokens = max_vocab_size_ - next_id;
    
    for (int i = 0; i < std::min(max_new_tokens, static_cast<int>(word_freq.size())); ++i) {
        const auto& word = word_freq[i].first;
        if (token_to_id_.find(word) == token_to_id_.end()) {
            token_to_id_[word] = next_id;
            id_to_token_[next_id] = word;
            next_id++;
        }
    }
    
    std::cout << "Vocabulary size: " << token_to_id_.size() << std::endl;
    is_trained_ = true;
}

std::vector<int> WordPieceTokenizer::tokenize(const std::string& text, 
                                             int max_len, 
                                             bool truncation,
                                             bool add_special_tokens,
                                             bool padding) {
    // Use the tokenize_with_attention method and just return the token IDs
    auto [token_ids, _] = tokenize_with_attention(text, max_len, truncation, add_special_tokens, padding);
    return token_ids;
}

std::pair<std::vector<int>, std::vector<int>> WordPieceTokenizer::tokenize_with_attention(
                                               const std::string& text, 
                                               int max_len, 
                                               bool truncation,
                                               bool add_special_tokens,
                                               bool padding) {
    if (!is_trained_) {
        throw std::runtime_error("Tokenizer is not trained. Call train() first.");
    }
    
    std::vector<int> token_ids;
    
    // Reserve space for [CLS] token if needed
    if (add_special_tokens) {
        token_ids.push_back(token_to_id_["[CLS]"]);
    }
    
    // Normalize and pre-tokenize the text
    bool is_uncased = true; // By default, use uncased normalization
    std::string normalized = normalize(text, is_uncased);
    std::vector<std::string> words = pre_tokenize(normalized);
    
    // Calculate max tokens to add (accounting for special tokens)
    int max_tokens_to_add = max_len;
    if (add_special_tokens) {
        max_tokens_to_add -= 2;  // for [CLS] and [SEP]
    }
    
    // Tokenize each word and add to result
    for (const auto& word : words) {
        auto word_tokens = word_piece_tokenize(word);
        
        // Check if adding these tokens would exceed max length
        if (truncation && token_ids.size() + word_tokens.size() > max_tokens_to_add) {
            // Add as many tokens as possible
            int remaining = max_tokens_to_add - token_ids.size();
            for (int i = 0; i < remaining; ++i) {
                token_ids.push_back(token_to_id_[word_tokens[i]]);
            }
            break;
        }
        
        // Add all tokens for this word
        for (const auto& token : word_tokens) {
            token_ids.push_back(token_to_id_[token]);
        }
    }
    
    // Add [SEP] token if needed
    if (add_special_tokens) {
        if (token_ids.size() < max_len) {
            token_ids.push_back(token_to_id_["[SEP]"]);
        } else if (truncation) {
            // Replace last token with [SEP]
            token_ids[max_len - 1] = token_to_id_["[SEP]"];
        }
    }
    
    // Create attention mask (1 for real tokens)
    std::vector<int> attention_mask(token_ids.size(), 1);
    
    // Pad if needed
    if (padding && token_ids.size() < max_len) {
        int pad_id = token_to_id_["[PAD]"];
        int original_size = token_ids.size();
        
        // Pad tokens with padding token
        token_ids.resize(max_len, pad_id);
        
        // Update attention mask (0 for padding tokens)
        attention_mask.resize(max_len, 0);
        for (int i = 0; i < original_size; ++i) {
            attention_mask[i] = 1;
        }
    }
    
    return {token_ids, attention_mask};
}

std::vector<std::vector<int>> WordPieceTokenizer::batch_tokenize(
                                      const std::vector<std::string>& texts,
                                      int max_len,
                                      bool truncation,
                                      bool add_special_tokens,
                                      bool padding,
                                      int num_threads) {
    // Use the batch_tokenize_with_attention method and extract only the token IDs
    auto results_with_attention = batch_tokenize_with_attention(
        texts, max_len, truncation, add_special_tokens, padding, num_threads);
    
    std::vector<std::vector<int>> results(texts.size());
    for (size_t i = 0; i < results_with_attention.size(); ++i) {
        results[i] = results_with_attention[i].first;
    }
    
    return results;
}

std::vector<std::pair<std::vector<int>, std::vector<int>>> WordPieceTokenizer::batch_tokenize_with_attention(
                                      const std::vector<std::string>& texts,
                                      int max_len,
                                      bool truncation,
                                      bool add_special_tokens,
                                      bool padding,
                                      int num_threads) {
    if (!is_trained_) {
        throw std::runtime_error("Tokenizer is not trained. Call train() first.");
    }
    
    std::vector<std::pair<std::vector<int>, std::vector<int>>> result(texts.size());
    
    // Single-threaded case
    if (num_threads <= 1 || texts.size() <= 1) {
        for (size_t i = 0; i < texts.size(); ++i) {
            result[i] = tokenize_with_attention(texts[i], max_len, truncation, add_special_tokens, padding);
        }
        return result;
    }
    
    // Multi-threaded case
    std::vector<std::future<void>> futures;
    int batch_size = (texts.size() + num_threads - 1) / num_threads;
    
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            int start = t * batch_size;
            int end = std::min(start + batch_size, static_cast<int>(texts.size()));
            
            for (int i = start; i < end; ++i) {
                result[i] = tokenize_with_attention(texts[i], max_len, truncation, add_special_tokens, padding);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    return result;
}

bool WordPieceTokenizer::save(const std::string& path) {
    try {
        nlohmann::json vocab;
        
        // Save token to id mapping
        for (const auto& [token, id] : token_to_id_) {
            vocab["token_to_id"][token] = id;
        }
        
        // Save special tokens
        vocab["special_tokens"] = special_tokens_;
        
        // Write to file
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for saving: " << path << std::endl;
            return false;
        }
        
        file << vocab.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving tokenizer: " << e.what() << std::endl;
        return false;
    }
}

bool WordPieceTokenizer::load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open tokenizer file: " << path << std::endl;
            return false;
        }
        
        nlohmann::json vocab;
        file >> vocab;
        file.close();
        
        // Clear existing vocabulary
        token_to_id_.clear();
        id_to_token_.clear();
        
        // Load token to id mapping
        for (auto it = vocab["token_to_id"].begin(); it != vocab["token_to_id"].end(); ++it) {
            int id = it.value().get<int>();
            std::string token = it.key();
            
            token_to_id_[token] = id;
            id_to_token_[id] = token;
        }
        
        // Load special tokens
        if (vocab.contains("special_tokens")) {
            special_tokens_ = vocab["special_tokens"].get<std::vector<std::string>>();
        }
        
        is_trained_ = true;
        std::cout << "Loaded tokenizer with vocabulary size: " << token_to_id_.size() << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading tokenizer: " << e.what() << std::endl;
        return false;
    }
}

bool WordPieceTokenizer::is_trained() const {
    return is_trained_;
}

int WordPieceTokenizer::get_pad_token_id() const {
    auto it = token_to_id_.find("[PAD]");
    if (it != token_to_id_.end()) {
        return it->second;
    }
    return 0; // Default to 0 if not found
}

std::string WordPieceTokenizer::normalize(const std::string& text, bool is_uncased) {
    // Basic normalization
    std::string result = text;
    
    // Convert to lowercase if uncased model
    if (is_uncased) {
        std::transform(result.begin(), result.end(), result.begin(), 
                    [](unsigned char c) { return std::tolower(c); });
    }
    
    // Remove control characters
    result.erase(std::remove_if(result.begin(), result.end(), 
                               [](unsigned char c) { return std::iscntrl(c); }),
                result.end());
    
    // Replace multiple spaces with single space
    std::regex multi_spaces("\\s+");
    result = std::regex_replace(result, multi_spaces, " ");
    
    // Trim leading and trailing whitespace
    result = std::regex_replace(result, std::regex("^\\s+|\\s+$"), "");
    
    return result;
}

std::vector<std::string> WordPieceTokenizer::pre_tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current_token;
    
    for (char c : text) {
        if (std::isalnum(c) || c == '\'' || c == '-') {
            // Part of a word
            current_token += c;
        } else {
            // Non-word character
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            
            if (!std::isspace(c)) {
                // Add non-space punctuation as a separate token
                tokens.push_back(std::string(1, c));
            }
        }
    }
    
    // Add the last token if there is one
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }
    
    return tokens;
}

std::vector<std::string> WordPieceTokenizer::word_piece_tokenize(const std::string& word) {
    // If word is in vocabulary, return it as is
    if (token_to_id_.find(word) != token_to_id_.end()) {
        return {word};
    }
    
    // Otherwise, break it into word pieces
    std::vector<std::string> word_pieces;
    
    // Maximum character length to consider for sub-words
    int max_char_length = word.length();
    
    // Try to find the longest subword at the beginning
    bool is_first = true;
    std::string remaining = word;
    
    while (!remaining.empty()) {
        bool found_subword = false;
        
        // Try decreasing subword lengths
        for (int end = remaining.length(); end > 0; --end) {
            std::string subword = remaining.substr(0, end);
            
            // Add the "##" prefix for non-first pieces
            if (!is_first) {
                subword = "##" + subword;
            }
            
            // Check if this subword is in vocabulary
            if (token_to_id_.find(subword) != token_to_id_.end()) {
                word_pieces.push_back(subword);
                remaining = remaining.substr(end);
                is_first = false;
                found_subword = true;
                break;
            }
        }
        
        // If no valid subword found, add [UNK] token
        if (!found_subword) {
            word_pieces.push_back("[UNK]");
            break;
        }
    }
    
    return word_pieces;
}

//==============================================================================
// LogBERTVectorizer Implementation
//==============================================================================

LogBERTVectorizer::LogBERTVectorizer(const LogBERTVectorizerConfig& config)
    : config_(config) {
    
    // Initialize special tokens
    special_tokens_ = {"[UNK]", "[PAD]", "[CLS]", "[SEP]", "[MASK]"};
    
    // Add custom tokens if provided
    if (!config_.custom_tokens.empty()) {
        special_tokens_.insert(special_tokens_.end(), 
                             config_.custom_tokens.begin(), 
                             config_.custom_tokens.end());
    }
    
    // Setup tokenizer directory
    std::string tokenizer_dirname = config_.model_name + "_tokenizer";
    if (config_.tokenizer_dirpath.empty()) {
        config_.tokenizer_dirpath = config_.output_dir + "/" + tokenizer_dirname;
    }
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(config_.tokenizer_dirpath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }
    
    // Check if tokenizer exists or create new one
    bool dir_exists = std::filesystem::exists(config_.tokenizer_dirpath);
    bool dir_empty = dir_exists && std::filesystem::is_empty(config_.tokenizer_dirpath);
    
    if (!dir_exists || dir_empty) {
        // Initialize the tokenizer
        tokenizer_ = std::make_unique<WordPieceTokenizer>("", 
                                                         config.max_vocab_size, 
                                                         config.custom_tokens);
    } else {
        // Try to load existing tokenizer
        tokenizer_ = std::make_unique<WordPieceTokenizer>();
        load_tokenizer();
    }
}

void LogBERTVectorizer::fit(const std::vector<std::string>& log_corpus) {
    // Skip if tokenizer already exists and is not empty
    if (std::filesystem::exists(config_.tokenizer_dirpath) && 
        !std::filesystem::is_empty(config_.tokenizer_dirpath) &&
        is_trained()) {
        std::cout << "Using existing tokenizer from: " << config_.tokenizer_dirpath << std::endl;
        return;
    }
    
    if (log_corpus.empty()) {
        std::cerr << "Empty corpus provided for training" << std::endl;
        return;
    }
    
    std::cout << "Cleaning dataset for training..." << std::endl;
    auto cleaned_corpus = _clean_dataset(log_corpus);
    
    std::cout << "Training tokenizer on " << cleaned_corpus.size() << " log entries..." << std::endl;
    tokenizer_->train(cleaned_corpus, config_.train_batch_size, config_.num_proc);
    
    // Save the trained tokenizer
    save_tokenizer();
}

std::vector<std::vector<int>> LogBERTVectorizer::transform(const std::vector<std::string>& log_entries) {
    // Use transform_with_attention but only return the token IDs
    auto result_with_attention = transform_with_attention(log_entries);
    
    std::vector<std::vector<int>> result(log_entries.size());
    for (size_t i = 0; i < result_with_attention.size(); ++i) {
        result[i] = result_with_attention[i].first;
    }
    
    return result;
}

std::vector<std::pair<std::vector<int>, std::vector<int>>> LogBERTVectorizer::transform_with_attention(
    const std::vector<std::string>& log_entries) {
    
    if (!is_trained()) {
        throw std::runtime_error("Tokenizer is not trained. Call fit() first or load a pre-trained tokenizer.");
    }
    
    std::cout << "Tokenizing " << log_entries.size() << " log entries..." << std::endl;
    
    // Clean dataset
    auto cleaned_entries = _clean_dataset(log_entries);
    
    // Prepare results vector
    std::vector<std::pair<std::vector<int>, std::vector<int>>> results(cleaned_entries.size());
    
    // Process in parallel if multiple threads requested
    if (config_.num_proc > 1 && cleaned_entries.size() > config_.num_proc) {
        std::vector<std::thread> threads;
        const size_t batch_size = cleaned_entries.size() / config_.num_proc;
        
        for (int t = 0; t < config_.num_proc; ++t) {
            size_t start_idx = t * batch_size;
            size_t end_idx = (t == config_.num_proc - 1) ? 
                           cleaned_entries.size() : 
                           (t + 1) * batch_size;
            
            threads.emplace_back(&LogBERTVectorizer::_process_batch,
                               this,
                               std::ref(cleaned_entries),
                               start_idx,
                               end_idx,
                               std::ref(results));
        }
        
        // Join all threads
        for (auto& thread : threads) {
            thread.join();
        }
    } else {
        // Single-threaded processing
        for (size_t i = 0; i < cleaned_entries.size(); ++i) {
            results[i] = _tokenize_function(cleaned_entries[i]);
        }
    }
    
    return results;
}

bool LogBERTVectorizer::save_tokenizer(const std::string& path) {
    std::string save_path = path.empty() ? config_.tokenizer_dirpath : path;
    save_path += "/tokenizer.json"; // Add filename
    
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(std::filesystem::path(save_path).parent_path());
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return false;
    }
    
    std::cout << "Saving tokenizer to: " << save_path << std::endl;
    return tokenizer_->save(save_path);
}

bool LogBERTVectorizer::load_tokenizer(const std::string& path) {
    std::string load_path = path.empty() ? config_.tokenizer_dirpath : path;
    
    // If path is a directory, append filename
    if (std::filesystem::is_directory(load_path)) {
        load_path += "/tokenizer.json";
    }
    
    std::cout << "Loading tokenizer from: " << load_path << std::endl;
    return tokenizer_->load(load_path);
}

bool LogBERTVectorizer::is_trained() const {
    return tokenizer_->is_trained();
}

void LogBERTVectorizer::_process_batch(
    const std::vector<std::string>& log_entries,
    size_t start_idx,
    size_t end_idx,
    std::vector<std::pair<std::vector<int>, std::vector<int>>>& results) {
    
    for (size_t i = start_idx; i < end_idx && i < log_entries.size(); ++i) {
        std::lock_guard<std::mutex> lock(tokenizer_mutex_);
        results[i] = _tokenize_function(log_entries[i]);
    }
}

std::pair<std::vector<int>, std::vector<int>> LogBERTVectorizer::_tokenize_function(
    const std::string& log_line) {
    
    // Normalize the log line
    std::string normalized_line = _normalize_text(log_line);
    
    // Tokenize with attention mask
    return tokenizer_->tokenize_with_attention(
        normalized_line,
        config_.max_token_len,
        config_.truncation,
        true,  // Add special tokens
        true); // Add padding
}

std::string LogBERTVectorizer::_normalize_text(const std::string& text) const {
    std::string normalized = text;
    
    // Apply normalization based on model type
    bool is_uncased = config_.model_name.find("-uncased") != std::string::npos;
    
    if (is_uncased) {
        // Convert to lowercase for uncased models
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                     [](unsigned char c) { return std::tolower(c); });
    }
    
    return normalized;
}

std::vector<std::string> LogBERTVectorizer::_clean_dataset(const std::vector<std::string>& log_entries) {
    // Use the special tokens and patterns from the reference implementation
    std::unordered_set<std::string> tokens_to_remove = _get_all_special_tokens();
    
    std::vector<std::string> cleaned_entries;
    cleaned_entries.reserve(log_entries.size());
    
    for (const auto& line : log_entries) {
        // Split by space
        std::istringstream iss(line);
        std::vector<std::string> words;
        std::string word;
        
        while (iss >> word) {
            // Skip tokens that should be removed
            if (tokens_to_remove.find(word) == tokens_to_remove.end()) {
                words.push_back(word);
            }
        }
        
        // Only keep non-empty lines
        if (!words.empty()) {
            std::string cleaned_line;
            for (size_t i = 0; i < words.size(); ++i) {
                if (i > 0) cleaned_line += " ";
                cleaned_line += words[i];
            }
            
            // Apply additional cleaning patterns
            // Replace IP addresses with <IP> token
            cleaned_line = std::regex_replace(cleaned_line, 
                std::regex("\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b"), "<IP>");
            
            // Replace timestamps with <TIME> token
            cleaned_line = std::regex_replace(cleaned_line, 
                std::regex("\\b\\d{4}-\\d{2}-\\d{2}[T ]\\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)?(?:Z|[+-]\\d{2}:?\\d{2})?\\b"), 
                "<TIME>");
            
            // Replace file paths with <PATH> token
            cleaned_line = std::regex_replace(cleaned_line, 
                std::regex("\\b/[\\w/\\.\\-_]+\\b"), "<PATH>");
            
            // Replace hexadecimal numbers/hashes with <HEX> token
            cleaned_line = std::regex_replace(cleaned_line, 
                std::regex("\\b0[xX][0-9a-fA-F]+\\b|\\b[0-9a-fA-F]{8,}\\b"), "<HEX>");
            
            cleaned_entries.push_back(cleaned_line);
        }
    }
    
    return cleaned_entries;
}

std::unordered_set<std::string> LogBERTVectorizer::_get_all_special_tokens() const {
    std::unordered_set<std::string> all_tokens(
        special_tokens_.begin(), special_tokens_.end());
    
    // Add additional tokens to ignore as in reference implementation
    std::vector<std::string> ignore_tokens = {".", "*", ":", "$", "_", "-", "/"};
    all_tokens.insert(ignore_tokens.begin(), ignore_tokens.end());
    
    return all_tokens;
}

} // namespace logai 