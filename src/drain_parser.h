#pragma once

#include "log_parser.h"
#include "data_loader_config.h"
#include "template_store.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>
#include <unordered_set>

namespace logai {

// Forward declaration of the implementation class
class DrainParserImpl;

// String pool for interning common strings to reduce memory usage
class StringPool {
public:
    std::string_view intern(std::string_view str) {
        // Fast path for empty strings
        if (str.empty()) {
            static const std::string empty;
            return empty;
        }
        
        // Check if the string is already in the pool
        auto it = pool_.find(std::string(str));
        if (it != pool_.end()) {
            return *it;
        }
        
        // Insert the string into the pool
        auto result = pool_.insert(std::string(str));
        return *result.first;
    }
    
    size_t size() const {
        return pool_.size();
    }
    
private:
    std::unordered_set<std::string> pool_;
};

/**
 * DRAIN log parser - A high-performance implementation of the DRAIN log parsing algorithm
 * 
 * DRAIN is a log parsing algorithm that uses a fixed-depth parse tree to efficiently
 * group similar log messages and extract their templates and parameters.
 * 
 * This implementation is optimized for high-performance C++ processing.
 */
class DrainParser : public LogParser {
public:
    /**
     * Constructor for DrainParser
     * 
     * @param config The data loader configuration
     * @param depth The maximum depth of the parse tree (default: 4)
     * @param similarity_threshold The similarity threshold for grouping logs (default: 0.5)
     * @param max_children The maximum number of children per node (default: 100)
     */
    DrainParser(const DataLoaderConfig& config, 
                int depth = 4,
                double similarity_threshold = 0.5,
                int max_children = 100);
    
    /**
     * Destructor
     */
    ~DrainParser();
    
    /**
     * Parse a log line using the DRAIN algorithm
     * 
     * @param line The log line to parse
     * @return A LogRecordObject containing the parsed log
     */
    LogRecordObject parse_line(std::string_view line) override;

    /**
     * Set the maximum depth of the parse tree
     * 
     * @param depth The maximum depth (default: 4)
     */
    void setDepth(int depth);

    /**
     * Set the similarity threshold for grouping logs
     * 
     * @param threshold The similarity threshold (default: 0.5)
     */
    void setSimilarityThreshold(double threshold);
    
    /**
     * Search for log templates similar to a query
     * 
     * @param query The query string to search for
     * @param top_k The number of results to return (default: 10)
     * @return A vector of pairs containing template IDs and their similarity scores
     */
    std::vector<std::pair<int, float>> search_templates(const std::string& query, int top_k = 10);
    
    /**
     * Get logs for a specific template
     * 
     * @param template_id The template ID to get logs for
     * @return A vector of LogRecordObjects for the specified template
     */
    std::vector<LogRecordObject> get_logs_for_template(int template_id);
    
    /**
     * Save the template store to a file
     * 
     * @param path The path to save to
     * @return true if successful, false otherwise
     */
    bool save_templates(const std::string& path);
    
    /**
     * Load templates from a file
     * 
     * @param path The path to load from
     * @return true if successful, false otherwise
     */
    bool load_templates(const std::string& path);
    
    /**
     * Get the template store
     * 
     * @return A reference to the template store
     */
    const TemplateStore& get_template_store() const;

private:
    // Use PIMPL idiom to hide implementation details
    std::unique_ptr<DrainParserImpl> impl_;
    const DataLoaderConfig& config_;
    
    // Template store for storing and searching templates
    TemplateStore template_store_;
};

} // namespace logai 