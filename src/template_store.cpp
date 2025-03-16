#include "template_store.h"
#include <algorithm>
#include <fstream>
#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace logai {

TemplateStore::TemplateStore() {
    // Initialize with a default vectorizer
    LogBERTVectorizerConfig config;
    vectorizer_ = std::make_unique<LogBERTVectorizer>(config);
}

TemplateStore::~TemplateStore() = default;

bool TemplateStore::add_template(int template_id, const std::string& template_str, const LogRecordObject& log) {
    try {
        // Store the template
        templates_[template_id] = template_str;
        
        // Store the log
        if (template_logs_.find(template_id) == template_logs_.end()) {
            template_logs_[template_id] = std::vector<LogRecordObject>{log};
        } else {
            template_logs_[template_id].push_back(log);
        }
        
        // Generate and store the embedding
        embeddings_[template_id] = get_embedding(template_str);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error adding template: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::pair<int, float>> TemplateStore::search(const std::string& query, int top_k) {
    std::vector<std::pair<int, float>> results;
    
    if (templates_.empty()) {
        return results;
    }
    
    // Get embedding for the query
    std::vector<float> query_embedding = get_embedding(query);
    
    // Calculate similarity for each template
    for (const auto& [id, _] : templates_) {
        if (embeddings_.find(id) != embeddings_.end()) {
            float similarity = cosine_similarity(query_embedding, embeddings_[id]);
            results.emplace_back(id, similarity);
        }
    }
    
    // Sort by similarity (descending)
    std::sort(results.begin(), results.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Return top_k results
    if (results.size() > static_cast<size_t>(top_k)) {
        results.resize(top_k);
    }
    
    return results;
}

std::string TemplateStore::get_template(int template_id) const {
    auto it = templates_.find(template_id);
    if (it != templates_.end()) {
        return it->second;
    }
    return "";
}

std::vector<LogRecordObject> TemplateStore::get_logs(int template_id) const {
    auto it = template_logs_.find(template_id);
    if (it != template_logs_.end()) {
        return it->second;
    }
    return {};
}

bool TemplateStore::init_vectorizer(const LogBERTVectorizerConfig& config) {
    try {
        vectorizer_ = std::make_unique<LogBERTVectorizer>(config);
        
        // Clear the embedding cache since we have a new vectorizer
        embedding_cache_.clear();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing vectorizer: " << e.what() << std::endl;
        return false;
    }
}

bool TemplateStore::save(const std::string& path) const {
    try {
        json j;
        
        // Save templates
        for (const auto& [id, tmpl] : templates_) {
            j["templates"][std::to_string(id)] = tmpl;
        }
        
        // Save embeddings
        for (const auto& [id, emb] : embeddings_) {
            j["embeddings"][std::to_string(id)] = emb;
        }
        
        // Save to file
        std::ofstream file(path);
        file << j.dump(4);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving template store: " << e.what() << std::endl;
        return false;
    }
}

bool TemplateStore::load(const std::string& path) {
    try {
        // Open file
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << path << std::endl;
            return false;
        }
        
        // Parse JSON
        json j;
        file >> j;
        file.close();
        
        // Clear existing data
        templates_.clear();
        embeddings_.clear();
        
        // Load templates
        if (j.contains("templates")) {
            for (const auto& [id_str, tmpl] : j["templates"].items()) {
                int id = std::stoi(id_str);
                templates_[id] = tmpl.get<std::string>();
            }
        }
        
        // Load embeddings
        if (j.contains("embeddings")) {
            for (const auto& [id_str, emb] : j["embeddings"].items()) {
                int id = std::stoi(id_str);
                embeddings_[id] = emb.get<std::vector<float>>();
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading template store: " << e.what() << std::endl;
        return false;
    }
}

size_t TemplateStore::size() const {
    return templates_.size();
}

std::vector<float> TemplateStore::get_embedding(const std::string& text) {
    // Check if we have the embedding cached
    auto it = embedding_cache_.find(text);
    if (it != embedding_cache_.end()) {
        return it->second;
    }
    
    // If the vectorizer is not properly initialized, return a dummy embedding
    if (!vectorizer_) {
        // Return a simple dummy embedding (all zeros)
        std::vector<float> dummy_embedding(768, 0.0f);
        embedding_cache_[text] = dummy_embedding;
        return dummy_embedding;
    }
    
    try {
        // TODO: Implement actual embedding using vectorizer
        // For now, use a simple hash-based embedding as a placeholder
        std::vector<float> embedding(768, 0.0f);
        
        // Simple hash-based embedding
        size_t hash_val = std::hash<std::string>{}(text);
        for (size_t i = 0; i < 768; ++i) {
            // Use the hash to seed a simple pseudo-random number
            embedding[i] = static_cast<float>((hash_val + i * 31) % 1000) / 1000.0f;
        }
        
        // Normalize the embedding to unit length
        float norm = 0.0f;
        for (float val : embedding) {
            norm += val * val;
        }
        norm = std::sqrt(norm);
        
        if (norm > 0.0f) {
            for (float& val : embedding) {
                val /= norm;
            }
        }
        
        // Cache and return the result
        embedding_cache_[text] = embedding;
        return embedding;
    } catch (const std::exception& e) {
        std::cerr << "Error generating embedding: " << e.what() << std::endl;
        
        // Return a dummy embedding on error
        std::vector<float> dummy_embedding(768, 0.0f);
        embedding_cache_[text] = dummy_embedding;
        return dummy_embedding;
    }
}

float TemplateStore::cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) const {
    if (v1.empty() || v2.empty() || v1.size() != v2.size()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_v1 = 0.0f;
    float norm_v2 = 0.0f;
    
    for (size_t i = 0; i < v1.size(); ++i) {
        dot_product += v1[i] * v2[i];
        norm_v1 += v1[i] * v1[i];
        norm_v2 += v2[i] * v2[i];
    }
    
    if (norm_v1 <= 0.0f || norm_v2 <= 0.0f) {
        return 0.0f;
    }
    
    return dot_product / (std::sqrt(norm_v1) * std::sqrt(norm_v2));
}

} // namespace logai 