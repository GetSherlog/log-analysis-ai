#pragma once

#include "log_parser.h"
#include "data_loader_config.h"
#include <regex>

namespace logai {

class RegexParser : public LogParser {
public:
    RegexParser(const DataLoaderConfig& config, const std::string& pattern);
    LogRecordObject parse_line(std::string_view line) override;

private:
    const DataLoaderConfig& config_;
    std::regex pattern_;
};
}