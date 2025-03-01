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
        
        // Preprocess the log line if needed
        std::string_view preprocessed_line = preprocess_log(line);
        
        // Tokenize the log message
        std::vector<std::string> tokens = tokenize(preprocessed_line);
        
        // Intern common strings to reduce memory usage
        for (auto& token : tokens) {
            token = std::string(string_pool_.intern(token));
        }
        
        // Match or create a new log cluster
        auto cluster = match_log_message(tokens);
        
        // Extract parameters from the log message
        std::unordered_map<std::string, std::string> parameters;
        extract_parameters(tokens, cluster, parameters);
        
        // Fill the log record
        record.body = std::string(preprocessed_line);
        record.template_str = cluster->log_template;  // Set the template string
        record.attributes = std::move(parameters);
        
        // Extract timestamp and severity if available
        extract_metadata(line, record, config);
        
        return record;
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
        // Check for empty string_view to avoid issues
        if (line.empty()) {
            static const std::string empty;
            return empty;
        }
        
        try {
            // Return the string_view directly instead of copying
            return line;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cerr << "Error in preprocess_log function: " << e.what() << std::endl;
            static const std::string empty;
            return empty;
        }
    }
    
    std::shared_ptr<LogCluster> match_log_message(const std::vector<std::string>& tokens) {
        // If the log is empty, return a default cluster
        if (tokens.empty()) {
            std::lock_guard<std::mutex> lock(parser_mutex_);
            auto cluster = std::make_shared<LogCluster>(cluster_id_counter_++, std::vector<std::string>());
            return cluster;
        }
        
        // Find the leaf node for this log message
        std::shared_ptr<Node> current_node = root_;
        
        // Traverse the tree based on token positions
        for (int i = 0; i < std::min(depth_, static_cast<int>(tokens.size())); ++i) {
            // For the last level, use the length of the log message
            std::string key;
            if (i == depth_ - 1) {
                key = std::to_string(tokens.size());
            } else {
                // For other levels, use the token at the position
                key = tokens[i];
                // If the token is a number, replace with a special token
                if (is_number(key)) {
                    key = "<NUM>";
                }
            }
            
            // If the key doesn't exist, create a new node
            std::lock_guard<std::mutex> lock(parser_mutex_);
            if (current_node->children.find(key) == current_node->children.end()) {
                current_node->children[key] = std::make_shared<Node>();
            }
            
            current_node = current_node->children[key];
        }
        
        // Find the most similar cluster in the leaf node
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
            
            // If no cluster matches, create a new one
            if (matched_cluster == nullptr) {
                matched_cluster = std::make_shared<LogCluster>(cluster_id_counter_++, tokens);
                current_node->clusters.push_back(matched_cluster);
                
                // If we have too many clusters, split the node
                if (current_node->clusters.size() > max_children_) {
                    split_node(current_node);
                }
            } else {
                // Update the template of the matched cluster
                update_template(matched_cluster, tokens);
            }
        }
        
        return matched_cluster;
    }
    
    double calculate_similarity(const std::vector<std::string>& tokens1, const std::vector<std::string>& tokens2) {
        // If the lengths are different, they can't be very similar
        if (tokens1.size() != tokens2.size()) {
            return 0.0;
        }
        
        // Count the number of matching tokens
        int matching_tokens = 0;
        for (size_t i = 0; i < tokens1.size(); ++i) {
            if (tokens1[i] == tokens2[i] || tokens2[i] == WILDCARD) {
                matching_tokens++;
            }
        }
        
        // Calculate similarity as the ratio of matching tokens
        return static_cast<double>(matching_tokens) / tokens1.size();
    }
    
    void update_template(std::shared_ptr<LogCluster> cluster, const std::vector<std::string>& tokens) {
        // Update the template by replacing non-matching tokens with wildcards
        bool template_changed = false;
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i < cluster->tokens.size()) {
                // If the tokens don't match and it's not already a wildcard, replace with wildcard
                if (cluster->tokens[i] != tokens[i] && cluster->tokens[i] != WILDCARD) {
                    // Mark this position as a parameter
                    cluster->parameter_indices.insert(i);
                    
                    // Replace with wildcard
                    cluster->tokens[i] = WILDCARD;
                    template_changed = true;
                }
            }
        }
        
        // Only update the template if it changed
        if (template_changed) {
            cluster->update_template();
        }
    }
    
    void split_node(std::shared_ptr<Node> node) {
        // This is a simplified implementation of node splitting
        // In a full implementation, we would use a more sophisticated algorithm
        // to split the node based on the distribution of tokens
        
        // For now, we'll just keep the most recent clusters
        if (node->clusters.size() > max_children_) {
            // Sort clusters by ID (most recent first)
            std::sort(node->clusters.begin(), node->clusters.end(),
                [](const std::shared_ptr<LogCluster>& a, const std::shared_ptr<LogCluster>& b) {
                    return a->id > b->id;
                });
            
            // Keep only the most recent clusters
            node->clusters.resize(max_children_);
        }
    }
    
    void extract_parameters(const std::vector<std::string>& tokens, 
                           std::shared_ptr<LogCluster> cluster,
                           std::unordered_map<std::string, std::string>& parameters) {
        // Extract parameters based on parameter indices
        for (size_t i : cluster->parameter_indices) {
            if (i < tokens.size()) {
                // Use position as parameter name
                parameters["param_" + std::to_string(i)] = tokens[i];
            }
        }
    }
    
    void extract_metadata(std::string_view line, LogRecordObject& record, const DataLoaderConfig& config) {
        // Extract timestamp and severity if a pattern is provided
        if (!config.log_pattern.empty() && !line.empty()) {
            try {
                // Create a safe copy of the string_view
                std::string line_str;
                line_str.reserve(line.size());
                line_str.assign(line.data(), line.size());
                
                std::regex pattern(config.log_pattern);
                std::smatch matches;
                
                if (std::regex_search(line_str, matches, pattern)) {
                    // Extract timestamp if available
                    for (size_t i = 0; i < matches.size(); ++i) {
                        std::string group_name = std::to_string(i);
                        
                        if (group_name == "timestamp" || i == 1) { // Assume first group is timestamp
                            record.timestamp = parse_timestamp(matches[i].str(), config.datetime_format);
                        } else if (group_name == "severity" || i == 2) { // Assume second group is severity
                            record.severity = matches[i].str();
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(log_mutex);
                std::cerr << "Error in extract_metadata function: " << e.what() << std::endl;
            }
        }
    }
};

// DrainParser implementation
DrainParser::DrainParser(const DataLoaderConfig& config, int depth, double similarity_threshold, int max_children)
    : config_(config), impl_(std::make_unique<DrainParserImpl>(depth, similarity_threshold, max_children)) {}

DrainParser::~DrainParser() = default;

LogRecordObject DrainParser::parse_line(std::string_view line) {
    return impl_->parse(line, config_);
}

} // namespace logai 