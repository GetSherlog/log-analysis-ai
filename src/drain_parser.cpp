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

namespace logai {

// Forward declaration for timestamp parsing function
std::optional<std::chrono::system_clock::time_point> parse_timestamp(std::string_view timestamp, const std::string& format);

// Constants for the DRAIN algorithm
constexpr char WILDCARD[] = "<*>";

// Utility functions
std::vector<std::string> tokenize(std::string_view str, char delimiter = ' ') {
    std::vector<std::string> tokens;
    std::string str_copy(str);
    std::istringstream tokenStream(str_copy);
    std::string token;
    
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
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
    std::unordered_map<size_t, std::unordered_set<std::string>> parameters;
    
    LogCluster(int id, const std::vector<std::string>& tokens) 
        : id(id), tokens(tokens) {
        // Initialize the template with the tokens
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
        std::string preprocessed_line = preprocess_log(line);
        
        // Tokenize the log message
        std::vector<std::string> tokens = tokenize(preprocessed_line);
        
        // Match or create a new log cluster
        auto cluster = match_log_message(tokens);
        
        // Extract parameters from the log message
        std::unordered_map<std::string, std::string> parameters;
        extract_parameters(tokens, cluster, parameters);
        
        // Fill the log record
        record.body = preprocessed_line;
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
    
    std::string preprocess_log(std::string_view line) {
        // Simple preprocessing: convert to string
        return std::string(line);
    }
    
    std::shared_ptr<LogCluster> match_log_message(const std::vector<std::string>& tokens) {
        // If the log is empty, return a default cluster
        if (tokens.empty()) {
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
            if (current_node->children.find(key) == current_node->children.end()) {
                current_node->children[key] = std::make_shared<Node>();
            }
            
            current_node = current_node->children[key];
        }
        
        // Find the most similar cluster in the leaf node
        std::shared_ptr<LogCluster> matched_cluster = nullptr;
        double max_similarity = -1.0;
        
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
        std::vector<std::string> new_template = cluster->tokens;
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i < new_template.size()) {
                // If the tokens don't match and it's not already a wildcard, replace with wildcard
                if (new_template[i] != tokens[i] && new_template[i] != WILDCARD) {
                    // Store the parameter value
                    if (cluster->parameters.find(i) == cluster->parameters.end()) {
                        cluster->parameters[i] = std::unordered_set<std::string>();
                    }
                    cluster->parameters[i].insert(new_template[i]);
                    cluster->parameters[i].insert(tokens[i]);
                    
                    // Replace with wildcard
                    new_template[i] = WILDCARD;
                }
            }
        }
        
        // Update the cluster's tokens and template
        cluster->tokens = new_template;
        cluster->log_template = std::accumulate(new_template.begin(), new_template.end(), std::string(),
            [](const std::string& a, const std::string& b) {
                return a.empty() ? b : a + " " + b;
            });
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
        // Extract parameters based on the template
        for (size_t i = 0; i < std::min(tokens.size(), cluster->tokens.size()); ++i) {
            if (cluster->tokens[i] == WILDCARD) {
                // Use position as parameter name
                parameters["param_" + std::to_string(i)] = tokens[i];
            }
        }
    }
    
    void extract_metadata(std::string_view line, LogRecordObject& record, const DataLoaderConfig& config) {
        // Extract timestamp and severity if a pattern is provided
        if (!config.log_pattern.empty()) {
            std::regex pattern(config.log_pattern);
            std::string line_str(line);
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