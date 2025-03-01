#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <unordered_map>

namespace logai {

class LogRecordObject {
public:
    std::string body;
    std::string template_str;
    std::unordered_map<std::string, std::string> attributes;
    std::optional<std::string> severity;
    std::optional<std::chrono::system_clock::time_point> timestamp;
};

} 