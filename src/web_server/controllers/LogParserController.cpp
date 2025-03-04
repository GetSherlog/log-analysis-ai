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

LogParserController::LogParserController() 
    : drainParser_(std::make_unique<DrainParser>()) {
    // Initialize DrainParser with default settings
    // These can be made configurable as needed
    drainParser_->setDepth(4);
    drainParser_->setSimilarityThreshold(0.5);
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
    
    // Optional configuration parameters
    if (requestJson.contains("depth") && requestJson["depth"].is_number_integer()) {
        drainParser_->setDepth(requestJson["depth"].get<int>());
    }
    
    if (requestJson.contains("similarityThreshold") && requestJson["similarityThreshold"].is_number()) {
        drainParser_->setSimilarityThreshold(requestJson["similarityThreshold"].get<double>());
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
        logJson["parameters"] = log.parameters;
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
    
    // Determine file type and create appropriate parser
    std::unique_ptr<LogParser> parser;
    std::string fileExt = fs::path(filePath).extension().string();
    
    // Create data loader config
    DataLoaderConfig config;
    config.filePath = filePath;
    
    if (requestJson.contains("maxLines") && requestJson["maxLines"].is_number_integer()) {
        config.maxLines = requestJson["maxLines"].get<int>();
    }
    
    if (fileExt == ".json") {
        parser = std::make_unique<JsonParser>();
    } else if (fileExt == ".csv") {
        parser = std::make_unique<CsvParser>();
        
        // CSV parser configuration
        if (requestJson.contains("delimiter") && requestJson["delimiter"].is_string()) {
            std::string delimiter = requestJson["delimiter"].get<std::string>();
            static_cast<CsvParser*>(parser.get())->setDelimiter(delimiter[0]);
        }
        
        if (requestJson.contains("hasHeader") && requestJson["hasHeader"].is_boolean()) {
            bool hasHeader = requestJson["hasHeader"].get<bool>();
            static_cast<CsvParser*>(parser.get())->setHasHeader(hasHeader);
        }
    } else {
        // Check if Drain parsing is requested
        if (requestJson.contains("useDrainParser") && requestJson["useDrainParser"].is_boolean() && 
            requestJson["useDrainParser"].get<bool>()) {
            
            // Use DrainParser for this file
            parser = std::make_unique<DrainParser>(config);
            
            // Configure DrainParser if parameters are provided
            if (requestJson.contains("depth") && requestJson["depth"].is_number_integer()) {
                static_cast<DrainParser*>(parser.get())->setDepth(requestJson["depth"].get<int>());
            }
            
            if (requestJson.contains("similarityThreshold") && requestJson["similarityThreshold"].is_number()) {
                static_cast<DrainParser*>(parser.get())->setSimilarityThreshold(
                    requestJson["similarityThreshold"].get<double>());
            }
        } else {
            // Default to regex parser for other file types
            parser = std::make_unique<RegexParser>();
            
            // Configure regex pattern if provided
            if (requestJson.contains("pattern") && requestJson["pattern"].is_string()) {
                std::string pattern = requestJson["pattern"].get<std::string>();
                static_cast<RegexParser*>(parser.get())->setPattern(pattern);
            }
        }
    }
    
    // Create file data loader
    FileDataLoader dataLoader(config);
    dataLoader.setParser(std::move(parser));
    
    try {
        // Load and parse the file
        auto logRecords = dataLoader.load();
        
        // Prepare response
        json response;
        response["records"] = json::array();
        
        for (const auto& record : logRecords) {
            json recordJson;
            recordJson["timestamp"] = record.timestamp;
            recordJson["message"] = record.message;
            response["records"].push_back(recordJson);
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