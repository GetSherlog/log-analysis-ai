#include "json_parser.h"
#include <nlohmann/json.hpp>

namespace logai {

JsonParser::JsonParser(const DataLoaderConfig& config) : config_(config) {}

std::optional<std::chrono::system_clock::time_point> JsonParser::parse_timestamp(std::string_view timestamp, const std::string& format) {
    // Simple implementation - in a real application, this would parse the timestamp string
    // according to the provided format
    return std::nullopt;
}

LogRecordObject JsonParser::parse_line(std::string_view line) {
    LogRecordObject record;
    try {
        nlohmann::json json = nlohmann::json::parse(line);
        
        // Extract fields based on config dimensions
        for (const auto& dimension : config_.dimensions) {
            if (json.contains(dimension)) {
                if (dimension == "body") {
                    record.body = json[dimension].get<std::string>();
                } else if (dimension == "timestamp") {
                    record.timestamp = parse_timestamp(json[dimension].get<std::string>(), config_.datetime_format);
                } else if (dimension == "severity") {
                    record.severity = json[dimension].get<std::string>();
                } else {
                    // Store all other fields in attributes
                    record.attributes[dimension] = json[dimension].get<std::string>();
                }
            }
        }
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse JSON line: " + std::string(e.what()));
    }
    
    return record;
}
} 