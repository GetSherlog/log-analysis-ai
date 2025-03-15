#include "std_includes.h"
#include "csv_parser.h"
#include "simd_scanner.h"
#include <iomanip>

namespace logai {

CsvParser::CsvParser(const DataLoaderConfig& config) : config_(config) {}

// Helper function to parse timestamps based on format
std::optional<std::chrono::system_clock::time_point> parse_timestamp(std::string_view timestamp, const std::string& format) {
    std::tm tm = {};
    std::string timestamp_str(timestamp);
    std::istringstream ss(timestamp_str);
    
    ss >> std::get_time(&tm, format.c_str());
    
    if (ss.fail()) {
        return std::nullopt;
    }
    
    // Convert tm to time_t then to system_clock::time_point
    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        return std::nullopt;
    }
    
    return std::chrono::system_clock::from_time_t(time);
}

LogRecordObject CsvParser::parse_line(std::string_view line) {
    LogRecordObject record;
    std::vector<std::string_view> fields = split_line(line);

    // Map fields to record based on config
    for (size_t i = 0; i < fields.size() && i < config_.dimensions.size(); ++i) {
        const auto& field = fields[i];
        const auto& dimension = config_.dimensions[i];

        if (dimension == "body") {
            record.body = std::string(field);
        } else if (dimension == "timestamp") {
            record.timestamp = parse_timestamp(field, config_.datetime_format);
        } else if (dimension == "severity") {
            record.severity = std::string(field);
        } else {
            // Use unordered_map for attributes instead of using vector indexing
            record.attributes.insert({dimension, std::string(field)});
        }
    }

    return record;
}

std::vector<std::string_view> CsvParser::split_line(std::string_view line, char delimiter) {
    std::vector<std::string_view> fields;
    fields.reserve(16);

    if (config_.use_simd) {
        // Use SIMD-optimized CSV parsing
        auto scanner = SimdLogScanner(line.data(), line.size());
        size_t start = 0;
        
        while (!scanner.atEnd()) {
            size_t pos = scanner.findChar(delimiter);
            if (pos == std::string::npos) {
                // Last field
                fields.push_back(std::string_view(line.data() + start, line.size() - start));
                break;
            }
            fields.push_back(std::string_view(line.data() + start, pos - start));
            start = pos + 1;
            scanner.advance(pos + 1);
        }
    } else {
        // Fallback to standard parsing
        size_t start = 0;
        size_t pos = 0;
        
        while ((pos = line.find(delimiter, start)) != std::string::npos) {
            fields.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }
        fields.push_back(line.substr(start));
    }

    return fields;
}
} 