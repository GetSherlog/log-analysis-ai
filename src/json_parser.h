#pragma once

#include "log_parser.h"
#include "data_loader_config.h"
#include <string>
#include <map>
#include <chrono>
#include <optional>

namespace logai {

class JsonParser : public LogParser {
public:
    explicit JsonParser(const DataLoaderConfig& config);
    LogRecordObject parse_line(std::string_view line) override;

private:
    DataLoaderConfig config_;
    std::map<std::string, std::string> parse_json(std::string_view json_str);
    std::optional<std::chrono::system_clock::time_point> parse_timestamp(std::string_view timestamp, const std::string& format);
};
}