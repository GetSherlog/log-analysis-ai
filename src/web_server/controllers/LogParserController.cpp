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
    
    // Load templates if they exist
    if (fs::exists(template_store_path_)) {
        try {
            drainParser_->load_templates(template_store_path_);
            spdlog::info("Loaded templates from {}", template_store_path_);
        } catch (const std::exception& e) {
            spdlog::error("Error loading templates: {}", e.what());
        }
    }
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
    
    // Save templates after parsing
    drainParser_->save_templates(template_store_path_);
    
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
    
    // Check if the request contains parser type
    std::string parserType = "drain";  // Default to drain
    if (requestJson.contains("parserType") && requestJson["parserType"].is_string()) {
        parserType = requestJson["parserType"].get<std::string>();
    }
    
    try {
        // Load the file
        FileDataLoader dataLoader(config);
        
        // Parse the file
        std::vector<LogRecordObject> parsedLogs;
        
        if (parserType == "json") {
            // JSON parser handling
            JSONParser jsonParser;
            parsedLogs = dataLoader.load_and_parse(jsonParser);
        } else if (parserType == "csv") {
            // CSV parser handling
            CSVParser csvParser;
            parsedLogs = dataLoader.load_and_parse(csvParser);
        } else if (parserType == "regex") {
            // Regex parser handling (if it exists)
            RegexParser regexParser;
            parsedLogs = dataLoader.load_and_parse(regexParser);
        } else {
            // Default to drain
            parsedLogs = dataLoader.load_and_parse(*drainParser_);
        }
        
        // Prepare response
        json response;
        response["parsedLogs"] = json::array();
        
        // Limit the response to a reasonable size
        const size_t maxLogs = 1000;
        const size_t logsToReturn = std::min(parsedLogs.size(), maxLogs);
        
        for (size_t i = 0; i < logsToReturn; ++i) {
            const auto& log = parsedLogs[i];
            json logJson;
            logJson["template"] = log.template_str;
            logJson["body"] = log.body;
            logJson["attributes"] = log.attributes;
            response["parsedLogs"].push_back(logJson);
        }
        
        response["totalLogs"] = parsedLogs.size();
        response["returnedLogs"] = logsToReturn;
        
        // Save templates after parsing
        drainParser_->save_templates(template_store_path_);
        
        callback(createJsonResponse(response));
        
    } catch (const std::exception& e) {
        callback(createErrorResponse("Error parsing file: " + std::string(e.what())));
    }
}

void LogParserController::searchTemplates(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("query") || !requestJson["query"].is_string()) {
        callback(createErrorResponse("Request must contain 'query' string"));
        return;
    }
    
    std::string query = requestJson["query"].get<std::string>();
    
    // Get top_k parameter if present
    int top_k = 10;  // Default value
    if (requestJson.contains("topK") && requestJson["topK"].is_number_integer()) {
        top_k = requestJson["topK"].get<int>();
        top_k = std::max(1, std::min(100, top_k));  // Clamp between 1 and 100
    }
    
    // Search for similar templates
    auto results = drainParser_->search_templates(query, top_k);
    
    // Prepare response
    json response;
    response["results"] = json::array();
    
    for (const auto& [template_id, similarity] : results) {
        json result;
        result["templateId"] = template_id;
        result["similarity"] = similarity;
        result["template"] = drainParser_->get_template_store().get_template(template_id);
        
        // Get a sample of logs for this template (limit to 5)
        auto logs = drainParser_->get_logs_for_template(template_id);
        json logSamples = json::array();
        
        const size_t max_samples = 5;
        const size_t samples_to_return = std::min(logs.size(), max_samples);
        
        for (size_t i = 0; i < samples_to_return; ++i) {
            logSamples.push_back(logs[i].body);
        }
        
        result["logSamples"] = logSamples;
        result["totalLogs"] = logs.size();
        
        response["results"].push_back(result);
    }
    
    response["query"] = query;
    response["totalResults"] = results.size();
    
    callback(createJsonResponse(response));
}

} // namespace web
} // namespace logai 