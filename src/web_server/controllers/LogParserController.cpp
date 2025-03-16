#include "LogParserController.h"
#include "json_parser.h"
#include "csv_parser.h"
#include "regex_parser.h"
#include "drain_parser.h"
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace logai {
namespace web {

LogParserController::LogParserController() {
    // Initialize DrainParser with default settings
    DataLoaderConfig config;
    drainParser_ = std::make_unique<DrainParser>(config, 4, 0.5, 100);
}

void LogParserController::parseDrain(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("logLines") || !requestJson["logLines"].is_array()) {
        callback(createErrorResponse("Request must contain 'logLines' array"));
        return;
    }
    
    // Extract log lines
    std::vector<std::string> logLines;
    for (const auto& line : requestJson["logLines"]) {
        if (line.is_string()) {
            logLines.push_back(line.get<std::string>());
        }
    }
    
    if (logLines.empty()) {
        callback(createErrorResponse("No valid log lines provided"));
        return;
    }
    
    // Parse the log lines
    std::vector<LogRecordObject> parsedLogs;
    for (const auto& line : logLines) {
        parsedLogs.push_back(drainParser_->parse_line(line));
    }
    
    // Prepare response
    json response;
    response["parsedLogs"] = json::array();
    
    for (const auto& log : parsedLogs) {
        json logJson;
        logJson["template"] = log.template_str;
        logJson["attributes"] = log.attributes;
        response["parsedLogs"].push_back(logJson);
    }
    
    response["totalLogs"] = logLines.size();
    
    callback(createJsonResponse(response));
}

void LogParserController::parseFile(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("filePath") || !requestJson["filePath"].is_string()) {
        callback(createErrorResponse("Request must contain 'filePath' string"));
        return;
    }
    
    std::string filePath = requestJson["filePath"].get<std::string>();
    
    // Check if file exists
    if (!fs::exists(filePath)) {
        callback(createErrorResponse("File not found: " + filePath, drogon::k404NotFound));
        return;
    }
    
    // Create data loader config
    DataLoaderConfig config;
    config.file_path = filePath;
    
    if (requestJson.contains("maxLines") && requestJson["maxLines"].is_number_integer()) {
        config.max_lines = requestJson["maxLines"].get<int>();
    }
    
    // Create file data loader and DrainParser for template extraction
    FileDataLoader dataLoader(config);
    
    try {
        // Load and parse the file
        auto logRecords = dataLoader.load_data();
        
        // Prepare response
        json response;
        folly::F14FastSet<std::string_view> templates;
        response["templates"] = json::array();
        
        for (const auto& record : logRecords) {
            json recordJson;
            if (record.timestamp) {
                auto timestamp_value = std::chrono::duration_cast<std::chrono::milliseconds>(
                    record.timestamp->time_since_epoch()).count();
                recordJson["timestamp"] = timestamp_value;
            }
            recordJson["message"] = record.body;
            if (record.severity) {
                recordJson["severity"] = *record.severity;
            }
            recordJson["attributes"] = record.attributes;
            templates.insert(record.template_str);
        }
        
        for (const auto& template : templates) {
            json templateJson;
            templateJson["template"] = template;
            response["templates"].push_back(templateJson);
        }
        
        response["totalRecords"] = logRecords.size();
        response["filePath"] = filePath;
        
        callback(createJsonResponse(response));
    } catch (const std::exception& e) {
        callback(createErrorResponse("Error parsing file: " + std::string(e.what())));
    }
}

} // namespace web
} // namespace logai 