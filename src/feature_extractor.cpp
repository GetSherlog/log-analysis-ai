#include "feature_extractor.h"
#include <algorithm>
#include <execution>
#include <numeric>
#include <unordered_map>
#include <chrono>
#include <string>
#include <sstream>
#include <iostream>
#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/builder.h>
#include <absl/strings/str_join.h>

namespace logai {

namespace {
// Helper function to parse time frequency string (e.g., "1s", "1m", "1h")
std::chrono::seconds parse_time_frequency(const std::string& freq) {
    if (freq.empty()) {
        return std::chrono::seconds(0);
    }
    
    size_t value_end = 0;
    int value = std::stoi(freq, &value_end);
    
    char unit = freq[value_end];
    switch (unit) {
        case 's': return std::chrono::seconds(value);
        case 'm': return std::chrono::minutes(value);
        case 'h': return std::chrono::hours(value);
        case 'd': return std::chrono::hours(value * 24);
        default:  return std::chrono::seconds(value);
    }
}

// Helper function to floor a timestamp to the given frequency
std::chrono::system_clock::time_point floor_time(
    const std::chrono::system_clock::time_point& tp, 
    const std::chrono::seconds& freq) {
    
    if (freq.count() == 0) {
        return tp;
    }
    
    auto duration = tp.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    
    // Calculate floored seconds
    seconds = std::chrono::seconds(seconds.count() / freq.count() * freq.count());
    
    return std::chrono::system_clock::time_point(seconds);
}

// Helper to create a group key from attributes and timestamp
std::unordered_map<std::string, std::string> create_group_key(
    const LogRecordObject& log,
    const std::vector<std::string>& group_by_category,
    const std::chrono::system_clock::time_point& time_bucket) {
    
    std::unordered_map<std::string, std::string> key;
    
    // Add category attributes to key
    for (const auto& category : group_by_category) {
        auto it = log.attributes.find(category);
        if (it != log.attributes.end()) {
            key[category] = it->second;
        } else {
            key[category] = "";  // Use empty string for missing values
        }
    }
    
    // Add time bucket to key if applicable
    if (time_bucket != std::chrono::system_clock::time_point{}) {
        // Format time bucket as ISO 8601 string
        auto time_t = std::chrono::system_clock::to_time_t(time_bucket);
        std::tm tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
        key["timestamp"] = buffer;
    }
    
    return key;
}

// Helper to convert a vector to an Arrow array
template<typename T>
std::shared_ptr<arrow::Array> vector_to_arrow_array(const std::vector<T>& vec) {
    arrow::NumericBuilder<typename arrow::CTypeTraits<T>::ArrowType> builder;
    auto status = builder.AppendValues(vec);
    if (!status.ok()) {
        return nullptr;
    }
    std::shared_ptr<arrow::Array> array;
    status = builder.Finish(&array);
    if (!status.ok()) {
        return nullptr;
    }
    return array;
}

} // anonymous namespace

FeatureExtractor::FeatureExtractor(const FeatureExtractorConfig& config)
    : config_(config) {
}

std::chrono::system_clock::time_point FeatureExtractor::get_time_bucket(
    const std::chrono::system_clock::time_point& timestamp) {
    
    if (config_.group_by_time.empty()) {
        return std::chrono::system_clock::time_point{};
    }
    
    auto freq = parse_time_frequency(config_.group_by_time);
    return floor_time(timestamp, freq);
}

std::vector<std::pair<std::unordered_map<std::string, std::string>, std::vector<size_t>>>
FeatureExtractor::group_logs(const std::vector<LogRecordObject>& logs) {
    // Map to accumulate indices by group key
    std::unordered_map<std::string, std::pair<std::unordered_map<std::string, std::string>, std::vector<size_t>>> groups;
    
    // Parse time frequency once
    auto time_freq = parse_time_frequency(config_.group_by_time);
    
    // Process each log record
    for (size_t i = 0; i < logs.size(); ++i) {
        const auto& log = logs[i];
        
        // Get time bucket if needed
        std::chrono::system_clock::time_point time_bucket;
        if (!config_.group_by_time.empty() && log.timestamp) {
            time_bucket = floor_time(*log.timestamp, time_freq);
        }
        
        // Create group key
        auto key_map = create_group_key(log, config_.group_by_category, time_bucket);
        
        // Create string representation of key for map lookup
        std::string key_str;
        for (const auto& [k, v] : key_map) {
            key_str += k + ":" + v + ";";
        }
        
        // Add to group
        auto& group = groups[key_str];
        group.first = std::move(key_map);
        group.second.push_back(i);
    }
    
    // Convert map to vector of pairs
    std::vector<std::pair<std::unordered_map<std::string, std::string>, std::vector<size_t>>> result;
    result.reserve(groups.size());
    
    for (auto& [_, group] : groups) {
        result.emplace_back(std::move(group.first), std::move(group.second));
    }
    
    return result;
}

std::vector<std::pair<std::unordered_map<std::string, std::string>, std::vector<size_t>>>
FeatureExtractor::apply_sliding_window(
    const std::vector<std::pair<std::unordered_map<std::string, std::string>, std::vector<size_t>>>& grouped_logs) {
    
    if (config_.sliding_window <= 0) {
        return grouped_logs;
    }
    
    if (config_.steps <= 0) {
        throw std::runtime_error("Steps should be greater than zero. Steps: " + 
                                std::to_string(config_.steps));
    }
    
    std::vector<std::pair<std::unordered_map<std::string, std::string>, std::vector<size_t>>> result;
    
    for (const auto& [group_key, indices] : grouped_logs) {
        if (indices.size() <= static_cast<size_t>(config_.sliding_window)) {
            // If group size <= window size, keep as is
            result.emplace_back(group_key, indices);
        } else {
            // Apply sliding window
            for (size_t i = 0; i + config_.sliding_window <= indices.size(); i += config_.steps) {
                std::vector<size_t> window_indices(indices.begin() + i, 
                                                 indices.begin() + i + config_.sliding_window);
                result.emplace_back(group_key, std::move(window_indices));
            }
        }
    }
    
    return result;
}

FeatureExtractionResult FeatureExtractor::convert_to_counter_vector(
    const std::vector<LogRecordObject>& logs) {
    
    FeatureExtractionResult result;
    
    // Group logs based on configuration
    auto grouped_logs = group_logs(logs);
    
    // Apply sliding window if configured
    if (config_.sliding_window > 0) {
        grouped_logs = apply_sliding_window(grouped_logs);
    }
    
    // Prepare result structure
    result.event_indices.reserve(grouped_logs.size());
    result.group_identifiers.reserve(grouped_logs.size());
    result.counts.reserve(grouped_logs.size());
    
    // Fill result with grouped data
    for (const auto& [group_key, indices] : grouped_logs) {
        result.event_indices.push_back(indices);
        result.group_identifiers.push_back(group_key);
        result.counts.push_back(indices.size());
    }
    
    return result;
}

FeatureExtractionResult FeatureExtractor::convert_to_feature_vector(
    const std::vector<LogRecordObject>& logs,
    const std::shared_ptr<arrow::Table>& log_vectors) {
    
    FeatureExtractionResult result;
    
    // Group logs based on configuration
    auto grouped_logs = group_logs(logs);
    
    // Apply sliding window if configured
    if (config_.sliding_window > 0) {
        grouped_logs = apply_sliding_window(grouped_logs);
    }
    
    // Prepare result structure
    result.event_indices.reserve(grouped_logs.size());
    result.group_identifiers.reserve(grouped_logs.size());
    
    // Create feature vectors using Arrow
    if (log_vectors && log_vectors->num_rows() > 0) {
        std::vector<std::shared_ptr<arrow::ChunkedArray>> merged_features;
        merged_features.reserve(log_vectors->num_columns());
        
        // Initialize merged feature arrays
        for (int col_idx = 0; col_idx < log_vectors->num_columns(); ++col_idx) {
            merged_features.push_back(std::make_shared<arrow::ChunkedArray>(
                std::vector<std::shared_ptr<arrow::Array>>{}));
        }
        
        // Process each group
        for (const auto& [group_key, indices] : grouped_logs) {
            // Store group metadata
            result.event_indices.push_back(indices);
            result.group_identifiers.push_back(group_key);
            
            // Calculate mean of feature vectors for this group
            for (int col_idx = 0; col_idx < log_vectors->num_columns(); ++col_idx) {
                const auto& column = log_vectors->column(col_idx);
                
                // Collect values for this group
                std::vector<double> group_values;
                group_values.reserve(indices.size());
                
                for (size_t idx : indices) {
                    if (idx < static_cast<size_t>(column->length())) {
                        double value;
                        if (column->type()->id() == arrow::Type::DOUBLE) {
                            auto double_array = std::static_pointer_cast<arrow::NumericArray<arrow::DoubleType>>(column->chunk(0));
                            if (!double_array->IsNull(idx)) {
                                value = double_array->Value(idx);
                                group_values.push_back(value);
                            }
                        } else if (column->type()->id() == arrow::Type::INT64) {
                            auto int_array = std::static_pointer_cast<arrow::NumericArray<arrow::Int64Type>>(column->chunk(0));
                            if (!int_array->IsNull(idx)) {
                                value = static_cast<double>(int_array->Value(idx));
                                group_values.push_back(value);
                            }
                        }
                    }
                }
                
                // Calculate mean and add to merged features
                if (!group_values.empty()) {
                    double mean = std::accumulate(group_values.begin(), group_values.end(), 0.0) / 
                                  group_values.size();
                    
                    auto builder = std::make_shared<arrow::DoubleBuilder>();
                    auto status = builder->Append(mean);
                    if (!status.ok()) {
                        // Handle error
                        std::cerr << "Error appending value: " << status.ToString() << std::endl;
                    }
                    std::shared_ptr<arrow::Array> mean_array;
                    status = builder->Finish(&mean_array);
                    if (!status.ok()) {
                        continue;
                    }
                    
                    auto existing_chunks = merged_features[col_idx]->chunks();
                    std::vector<std::shared_ptr<arrow::Array>> new_chunks = existing_chunks;
                    new_chunks.push_back(mean_array);
                    
                    merged_features[col_idx] = std::make_shared<arrow::ChunkedArray>(new_chunks);
                }
            }
        }
        
        // Create Arrow table from merged features
        std::vector<std::shared_ptr<arrow::Field>> fields;
        for (int i = 0; i < log_vectors->num_columns(); ++i) {
            fields.push_back(arrow::field("feature_" + std::to_string(i), arrow::float64()));
        }
        
        auto schema = std::make_shared<arrow::Schema>(fields);
        result.feature_vectors = arrow::Table::Make(schema, merged_features);
    }
    
    return result;
}

FeatureExtractionResult FeatureExtractor::convert_to_sequence(
    const std::vector<LogRecordObject>& logs) {
    
    FeatureExtractionResult result;
    
    // Group logs based on configuration
    auto grouped_logs = group_logs(logs);
    
    // Apply sliding window if configured
    if (config_.sliding_window > 0) {
        grouped_logs = apply_sliding_window(grouped_logs);
    }
    
    // Prepare result structure
    result.event_indices.reserve(grouped_logs.size());
    result.group_identifiers.reserve(grouped_logs.size());
    result.sequences.reserve(grouped_logs.size());
    
    // Process each group
    for (const auto& [group_key, indices] : grouped_logs) {
        // Store group metadata
        result.event_indices.push_back(indices);
        result.group_identifiers.push_back(group_key);
        
        // Create sequence by joining log bodies
        std::vector<std::string> log_bodies;
        log_bodies.reserve(indices.size());
        
        for (size_t idx : indices) {
            if (idx < logs.size()) {
                log_bodies.push_back(logs[idx].body);
            }
        }
        
        // Join log bodies with spaces
        result.sequences.push_back(absl::StrJoin(log_bodies, " "));
    }
    
    return result;
}

} // namespace logai 