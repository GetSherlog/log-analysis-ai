#include "drain_parser.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_set>
#include <iostream>
#include <chrono>
#include <regex>
#include <numeric>
#include <mutex>

namespace logai {

// Mutex for thread-safe logging
static std::mutex log_mutex;

// Forward declaration for timestamp parsing function - make it static to avoid conflicts
static std::optional<std::chrono::system_clock::time_point> parse_timestamp(std::string_view timestamp, const std::string& format) {
    // Simple implementation that returns nullopt to avoid parsing errors
    return std::nullopt;
}

// Constants for the DRAIN algorithm
constexpr char WILDCARD[] = "<*>";

// Utility functions
std::vector<std::string> tokenize(std::string_view str, char delimiter = ' ') {
    std::vector<std::string> tokens;
    
    // Check for empty string_view to avoid issues
    if (str.empty()) {
        return tokens;
    }
    
    try {
        size_t start = 0;
        size_t end = 0;
        
        // Find each token by delimiter
        while ((end = str.find(delimiter, start)) != std::string_view::npos) {
            if (end > start) {  // Skip empty tokens
                tokens.emplace_back(str.substr(start, end - start));
            }
            start = end + 1;
        }
        
        // Add the last token if it exists
        if (start < str.size()) {
            tokens.emplace_back(str.substr(start));
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << "Error in tokenize function: " << e.what() << std::endl;
        // Return empty tokens on error
    }
    
    return tokens;
}

bool is_number(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), [](char c) {
        return std::isdigit(c) || c == '.' || c == '-' || c == '+';
    });
}

// DRAIN Node structure
struct LogCluster {
    int id;
    std::string log_template;
    std::vector<std::string> tokens;
    // Use a more memory-efficient way to store parameters
    // Instead of storing sets of strings, store parameter indices only
    std::unordered_set<size_t> parameter_indices;
    
    LogCluster(int id, const std::vector<std::string>& tokens) 
        : id(id), tokens(tokens) {
        // Initialize the template with the tokens - do this lazily to avoid duplicate work
        update_template();
    }
    
    void update_template() {
        // Regenerate the template from tokens
        log_template = std::accumulate(tokens.begin(), tokens.end(), std::string(),
            [](const std::string& a, const std::string& b) {
                return a.empty() ? b : a + " " + b;
            });
    }
};

struct Node {
    std::unordered_map<std::string, std::shared_ptr<Node>> children;
    std::vector<std::shared_ptr<LogCluster>> clusters;
    
    Node() = default;
};

// Implementation class for DrainParser
class DrainParserImpl {
public:
    DrainParserImpl(int depth, double similarity_threshold, int max_children)
        : depth_(depth), similarity_threshold_(similarity_threshold), max_children_(max_children), 
          root_(std::make_shared<Node>()), cluster_id_counter_(0) {}
    
    LogRecordObject parse(std::string_view line, const DataLoaderConfig& config) {
        LogRecordObject record;
        record.body = std::string(line);
        
        // Preprocess the log line
        std::string_view content = preprocess_log(line);
        
        // Tokenize the log content
        std::vector<std::string> tokens = tokenize(content);
        
        // Match with existing log clusters or create a new one
        std::shared_ptr<LogCluster> matched_cluster = match_log_message(tokens);
        
        // Extract and store the template
        record.template_str = matched_cluster->log_template;
        
        // Extract metadata (timestamp, severity, etc.)
        extract_metadata(line, record, config);
        
        return record;
    }
    
    // Return the cluster ID for a parsed log
    int get_cluster_id_for_log(std::string_view line) {
        // Preprocess the log line
        std::string_view content = preprocess_log(line);
        
        // Tokenize the log content
        std::vector<std::string> tokens = tokenize(content);
        
        // Match with existing log clusters or create a new one
        std::shared_ptr<LogCluster> matched_cluster = match_log_message(tokens);
        
        // Return the cluster ID
        return matched_cluster->id;
    }
    
    void setDepth(int depth) {
        depth_ = depth;
    }
    
    void setSimilarityThreshold(double threshold) {
        similarity_threshold_ = threshold;
    }
    
    // Get all templates
    std::unordered_map<int, std::string> get_all_templates() {
        std::lock_guard<std::mutex> lock(parser_mutex_);
        std::unordered_map<int, std::string> templates;
        collect_templates(root_, templates);
        return templates;
    }
    
private:
    int depth_;
    double similarity_threshold_;
    int max_children_;
    std::shared_ptr<Node> root_;
    int cluster_id_counter_;
    std::mutex parser_mutex_; // Mutex for thread safety
    StringPool string_pool_;  // String pool for memory optimization
    
    std::string_view preprocess_log(std::string_view line) {
        // For now, we'll just return the original content
        // In a real implementation, this would handle timestamp removal, etc.
        try {
            return line;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cerr << "Error preprocessing log: " << e.what() << std::endl;
            return "";
        }
    }
    
    std::shared_ptr<LogCluster> match_log_message(const std::vector<std::string>& tokens) {
        if (tokens.empty()) {
            // Create a default cluster for empty logs
            std::lock_guard<std::mutex> lock(parser_mutex_);
            auto cluster = std::make_shared<LogCluster>(cluster_id_counter_++, std::vector<std::string>{"<EMPTY>"});
            return cluster;
        }
        
        // Find the leaf node for this log message
        std::shared_ptr<Node> current_node = root_;
        
        // Descend the tree
        for (int depth = 0; depth < depth_; ++depth) {
            // Stop if we've reached a leaf
            if (current_node->children.empty()) {
                break;
            }
            
            // Get the token at the current depth or use the last token if we're out of bounds
            std::string token;
            if (depth < static_cast<int>(tokens.size())) {
                token = tokens[depth];
            } else {
                token = tokens.back();
            }
            
            // Follow the path in the tree
            auto child_it = current_node->children.find(token);
            if (child_it != current_node->children.end()) {
                current_node = child_it->second;
            } else {
                // No exact match at this depth, check for wildcard
                child_it = current_node->children.find(WILDCARD);
                if (child_it != current_node->children.end()) {
                    current_node = child_it->second;
                } else {
                    // No matching path, we've reached a leaf
                    break;
                }
            }
        }
        
        // Find the best cluster match at this node
        std::shared_ptr<LogCluster> matched_cluster = nullptr;
        double max_similarity = -1.0;
        
        {
            std::lock_guard<std::mutex> lock(parser_mutex_);
            for (const auto& cluster : current_node->clusters) {
                double similarity = calculate_similarity(tokens, cluster->tokens);
                if (similarity > max_similarity && similarity >= similarity_threshold_) {
                    max_similarity = similarity;
                    matched_cluster = cluster;
                }
            }
        }
        
        // If no match found, create a new cluster
        if (!matched_cluster) {
            std::lock_guard<std::mutex> lock(parser_mutex_);
            matched_cluster = std::make_shared<LogCluster>(cluster_id_counter_++, tokens);
            update_template(matched_cluster, tokens); // Initialize template
            current_node->clusters.push_back(matched_cluster);
            
            // If we have too many clusters, split the node
            if (static_cast<int>(current_node->clusters.size()) > max_children_) {
                split_node(current_node);
            }
        } else {
            // Update the existing template if necessary
            update_template(matched_cluster, tokens);
        }
        
        return matched_cluster;
    }
    
    double calculate_similarity(const std::vector<std::string>& tokens1, const std::vector<std::string>& tokens2) {
        if (tokens1.empty() || tokens2.empty()) {
            return 0.0;
        }
        
        const size_t len1 = tokens1.size();
        const size_t len2 = tokens2.size();
        
        // Use a simple token-by-token comparison
        size_t matched = 0;
        for (size_t i = 0; i < std::min(len1, len2); ++i) {
            if (tokens1[i] == tokens2[i] || tokens1[i] == WILDCARD || tokens2[i] == WILDCARD) {
                matched++;
            }
        }
        
        // Use max length for denominator to avoid rewarding shorter log messages
        return static_cast<double>(matched) / std::max(len1, len2);
    }
    
    void update_template(std::shared_ptr<LogCluster> cluster, const std::vector<std::string>& tokens) {
        if (tokens.empty()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(parser_mutex_);
        
        // First time seeing this cluster? Initialize with the tokens
        if (cluster->tokens.empty()) {
            cluster->tokens = tokens;
            extract_parameters(tokens, cluster);
            cluster->update_template();
            return;
        }
        
        // Update template tokens
        const size_t old_size = cluster->tokens.size();
        const size_t new_size = tokens.size();
        const size_t min_size = std::min(old_size, new_size);
        
        // Check for token mismatches and convert to wildcards
        for (size_t i = 0; i < min_size; ++i) {
            if (cluster->tokens[i] != tokens[i] && cluster->tokens[i] != WILDCARD) {
                if (is_number(cluster->tokens[i]) && is_number(tokens[i])) {
                    // Both are numbers, convert to wildcard
                    cluster->tokens[i] = WILDCARD;
                    cluster->parameter_indices.insert(i);
                } else if (cluster->tokens[i] != tokens[i]) {
                    // Different tokens, convert to wildcard
                    cluster->tokens[i] = WILDCARD;
                    cluster->parameter_indices.insert(i);
                }
            }
        }
        
        // Update template string
        cluster->update_template();
    }
    
    void split_node(std::shared_ptr<Node> node) {
        // Only split if we have lots of clusters
        if (node->clusters.size() <= 1) {
            return;
        }
        
        // Create child nodes based on the parameter positions
        for (const auto& cluster : node->clusters) {
            // Use the first token that's not a wildcard as key
            std::string key = WILDCARD;
            for (const auto& token : cluster->tokens) {
                if (token != WILDCARD) {
                    key = token;
                    break;
                }
            }
            
            // Create the child node if needed
            if (node->children.find(key) == node->children.end()) {
                node->children[key] = std::make_shared<Node>();
            }
            
            // Move the cluster to the child node
            node->children[key]->clusters.push_back(cluster);
        }
        
        // Clear the parent node's clusters
        node->clusters.clear();
    }
    
    void extract_parameters(const std::vector<std::string>& tokens, 
                           std::shared_ptr<LogCluster> cluster) {
        // Mark tokens that are likely parameters (e.g., numbers, hex, etc.)
        for (size_t i = 0; i < tokens.size(); ++i) {
            const std::string& token = tokens[i];
            if (is_number(token)) {
                cluster->parameter_indices.insert(i);
            }
        }
    }
    
    void extract_metadata(std::string_view line, LogRecordObject& record, const DataLoaderConfig& config) {
        // Extract timestamp if a format is provided
        if (!config.timestamp_format.empty() && config.timestamp_column >= 0) {
            // Split the line by the column delimiter
            std::vector<std::string> columns = tokenize(line, config.column_delimiter);
            
            // Check if the timestamp column exists
            if (static_cast<size_t>(config.timestamp_column) < columns.size()) {
                std::string timestamp_str = columns[config.timestamp_column];
                
                // Parse the timestamp
                auto timestamp = parse_timestamp(timestamp_str, config.timestamp_format);
                if (timestamp) {
                    record.timestamp = timestamp;
                }
            }
        }
        
        // Extract other metadata based on configuration
        // For now, this is a simplified placeholder
        try {
            record.attributes["source"] = "log";
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cerr << "Error extracting metadata: " << e.what() << std::endl;
        }
    }
    
    // Helper method to collect all templates from the tree
    void collect_templates(const std::shared_ptr<Node>& node, std::unordered_map<int, std::string>& templates) {
        // Add templates from this node
        for (const auto& cluster : node->clusters) {
            templates[cluster->id] = cluster->log_template;
        }
        
        // Recursively collect from child nodes
        for (const auto& [_, child] : node->children) {
            collect_templates(child, templates);
        }
    }
};

DrainParser::DrainParser(const DataLoaderConfig& config, int depth, double similarity_threshold, int max_children)
    : impl_(std::make_unique<DrainParserImpl>(depth, similarity_threshold, max_children)), 
      config_(config) {
}

DrainParser::~DrainParser() = default;

LogRecordObject DrainParser::parse_line(std::string_view line) {
    LogRecordObject record = impl_->parse(line, config_);
    
    // Store the template in our template store
    int cluster_id = impl_->get_cluster_id_for_log(line);
    template_store_.add_template(cluster_id, record.template_str, record);
    
    return record;
}

void DrainParser::setDepth(int depth) {
    impl_->setDepth(depth);
}

void DrainParser::setSimilarityThreshold(double threshold) {
    impl_->setSimilarityThreshold(threshold);
}

std::vector<std::pair<int, float>> DrainParser::search_templates(const std::string& query, int top_k) {
    return template_store_.search(query, top_k);
}

std::vector<LogRecordObject> DrainParser::get_logs_for_template(int template_id) {
    return template_store_.get_logs(template_id);
}

bool DrainParser::save_templates(const std::string& path) {
    return template_store_.save(path);
}

bool DrainParser::load_templates(const std::string& path) {
    return template_store_.load(path);
}

const TemplateStore& DrainParser::get_template_store() const {
    return template_store_;
}

} // namespace logai 