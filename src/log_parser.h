#pragma once

#include <string_view>
#include "log_record.h"

namespace logai {

class LogParser {
public:
    virtual ~LogParser() = default;
    virtual LogRecordObject parse_line(std::string_view line) = 0;
};

} 