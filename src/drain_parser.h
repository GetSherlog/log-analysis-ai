#pragma once

#include "log_parser.h"
#include "data_loader_config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>

namespace logai {

// Forward declaration of the implementation class
class DrainParserImpl;

/**
 * DRAIN log parser - A high-performance implementation of the DRAIN log parsing algorithm
 * 
 * DRAIN is a log parsing algorithm that uses a fixed-depth parse tree to efficiently
 * group similar log messages and extract their templates and parameters.
 * 
 * This implementation is optimized for high-performance C++ processing.
 */
class DrainParser : public LogParser {
public:
    /**
     * Constructor for DrainParser
     * 
     * @param config The data loader configuration
     * @param depth The maximum depth of the parse tree (default: 4)
     * @param similarity_threshold The similarity threshold for grouping logs (default: 0.5)
     * @param max_children The maximum number of children per node (default: 100)
     */
    DrainParser(const DataLoaderConfig& config, 
                int depth = 4,
                double similarity_threshold = 0.5,
                int max_children = 100);
    
    /**
     * Destructor
     */
    ~DrainParser();
    
    /**
     * Parse a log line using the DRAIN algorithm
     * 
     * @param line The log line to parse
     * @return A LogRecordObject containing the parsed log
     */
    LogRecordObject parse_line(std::string_view line) override;

private:
    // Use PIMPL idiom to hide implementation details
    std::unique_ptr<DrainParserImpl> impl_;
    const DataLoaderConfig& config_;
};

} // namespace logai 