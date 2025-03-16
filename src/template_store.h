#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "log_record.h"
#include "logbert_vectorizer.h"

namespace logai {

/**
 * TemplateStore - A class to store and search log templates using embeddings
 * 
 * This class stores log templates and their embeddings, and provides
 * functionality to search for templates similar to a given query.
 */
class TemplateStore {
public:
    /**
     * Constructor for TemplateStore
     */
    TemplateStore();
    
    /**
     * Destructor
     */
    ~TemplateStore();
    
    /**
     * Add a template to the store
     * 
     * @param template_id The template ID (usually cluster ID)
     * @param template_str The template string to store
     * @param log The original log record object associated with this template
     * @return true if successfully added, false otherwise
     */
    bool add_template(int template_id, const std::string& template_str, const LogRecordObject& log);
    
    /**
     * Search for templates similar to a query
     * 
     * @param query The query string to search for
     * @param top_k The number of results to return (default: 10)
     * @return A vector of pairs containing template IDs and their similarity scores
     */
    std::vector<std::pair<int, float>> search(const std::string& query, int top_k = 10);
    
    /**
     * Get a template by ID
     * 
     * @param template_id The template ID to retrieve
     * @return The template string, or empty string if not found
     */
    std::string get_template(int template_id) const;
    
    /**
     * Get logs for a template
     * 
     * @param template_id The template ID to retrieve logs for
     * @return A vector of log record objects associated with this template
     */
    std::vector<LogRecordObject> get_logs(int template_id) const;
    
    /**
     * Initialize the vectorizer with a custom model or tokenizer
     * 
     * @param config The LogBERT vectorizer configuration
     * @return true if successful, false otherwise
     */
    bool init_vectorizer(const LogBERTVectorizerConfig& config);
    
    /**
     * Save the template store to disk
     * 
     * @param path The path to save to
     * @return true if successful, false otherwise
     */
    bool save(const std::string& path) const;
    
    /**
     * Load the template store from disk
     * 
     * @param path The path to load from
     * @return true if successful, false otherwise
     */
    bool load(const std::string& path);
    
    /**
     * Get the number of templates in the store
     * 
     * @return The number of templates
     */
    size_t size() const;
    
private:
    // Map of template ID to template string
    std::unordered_map<int, std::string> templates_;
    
    // Map of template ID to logs
    std::unordered_map<int, std::vector<LogRecordObject>> template_logs_;
    
    // Map of template ID to embedding
    std::unordered_map<int, std::vector<float>> embeddings_;
    
    // Vectorizer for creating embeddings
    std::unique_ptr<LogBERTVectorizer> vectorizer_;
    
    // Cache for vectorizer to avoid repeated tokenization
    std::unordered_map<std::string, std::vector<float>> embedding_cache_;
    
    // Private helper methods
    std::vector<float> get_embedding(const std::string& text);
    float cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) const;
};

} // namespace logai 