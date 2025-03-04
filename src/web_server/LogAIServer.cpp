/**
 * @file LogAIServer.cpp
 * @brief Very simple web server for LogAI-CPP with Drogon
 */

#include <drogon/drogon.h>
#include <drogon/MultiPartParser.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>
#include <string>
#include <memory>
#include <filesystem>

// Include controllers
#include "controllers/AnomalyDetectionController.h"

namespace fs = std::filesystem;

// Server config constants
const int DEFAULT_PORT = 8080;
const int DEFAULT_THREAD_NUM = 2;
const std::string UPLOAD_PATH = "./uploads";
const std::string LOG_PATH = "./logs";

// Initialize logger
void init_logger() {
    try {
        // Create logs directory if it doesn't exist
        if (!fs::exists(LOG_PATH)) {
            fs::create_directory(LOG_PATH);
        }

        // Create console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // Create rotating file sink - 10MB size, 5 rotated files
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            LOG_PATH + "/logai_server.log", 10 * 1024 * 1024, 5);
        file_sink->set_level(spdlog::level::debug);

        // Create logger with both sinks
        auto logger = std::make_shared<spdlog::logger>("logai_server", 
            spdlog::sinks_init_list({console_sink, file_sink}));
        
        // Set global pattern
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v");
        
        // Set as default logger
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        
        spdlog::info("Logger initialized successfully");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        throw;
    }
}

// Define a simple CORS filter as a class
class CorsFilter : public drogon::HttpFilter<CorsFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                 drogon::FilterCallback &&fcb,
                 drogon::FilterChainCallback &&fccb) override {
        // Check if it's an OPTIONS request
        if (req->getMethod() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp->addHeader("Access-Control-Max-Age", "86400");
            fcb(resp);
            return;
        }
        
        // Continue processing the request
        fccb();
        
        // Add CORS headers to response
        if (req->getAttributes()->get<drogon::HttpResponsePtr>("response")) {
            auto resp = req->getAttributes()->get<drogon::HttpResponsePtr>("response");
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        }
    }
};

int main() {
    try {
        // Initialize logger
        init_logger();
        
        spdlog::info("Starting LogAI-CPP Web Server on port {} with {} threads...", 
                     DEFAULT_PORT, DEFAULT_THREAD_NUM);
        
        // Create upload directory if it doesn't exist
        if (!fs::exists(UPLOAD_PATH)) {
            fs::create_directory(UPLOAD_PATH);
            spdlog::info("Created upload directory: {}", UPLOAD_PATH);
        }
        
        // Create a simple health controller
        drogon::app().registerHandler(
            "/health",
            [](const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                spdlog::debug("Health check request received");
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"status\":\"ok\"}");
                callback(resp);
            }
        );
        
        // Create a simple API info controller
        drogon::app().registerHandler(
            "/api",
            [](const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                spdlog::debug("API info request received");
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"name\":\"LogAI-CPP API\",\"version\":\"0.1.0\",\"status\":\"running\"}");
                callback(resp);
            }
        );
        
        // Create a file upload handler
        drogon::app().registerHandler(
            "/api/upload",
            [](const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                spdlog::debug("File upload request received");
                auto resp = drogon::HttpResponse::newHttpResponse();
                
                // Check if this is a multipart/form-data request
                if (req->getContentType() != drogon::CT_MULTIPART_FORM_DATA) {
                    spdlog::warn("Invalid content type for file upload request");
                    resp->setStatusCode(drogon::k400BadRequest);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"error\":true,\"message\":\"Expecting multipart/form-data request\"}");
                    callback(resp);
                    return;
                }
                
                // Parse multipart form data
                auto &multipartParser = req->getMultipartParser();
                if (!multipartParser) {
                    spdlog::error("Failed to parse multipart form data");
                    resp->setStatusCode(drogon::k400BadRequest);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"error\":true,\"message\":\"Failed to parse multipart form data\"}");
                    callback(resp);
                    return;
                }
                
                const auto &files = multipartParser->getFiles();
                if (files.empty()) {
                    spdlog::warn("No files found in upload request");
                    resp->setStatusCode(drogon::k400BadRequest);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"error\":true,\"message\":\"No files uploaded\"}");
                    callback(resp);
                    return;
                }
                
                // First file in the request
                const auto &file = files[0];
                
                // Generate a timestamp for the filename
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();
                
                // Create a unique filename
                std::string filename = std::to_string(timestamp) + "_" + file.getFileName();
                std::string filepath = "./uploads/" + filename;
                
                // Save the file
                try {
                    file.saveAs(filepath);
                    spdlog::info("File saved successfully: {}", filepath);
                } catch (const std::exception& e) {
                    spdlog::error("Failed to save file {}: {}", filepath, e.what());
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"error\":true,\"message\":\"Failed to save uploaded file\"}");
                    callback(resp);
                    return;
                }
                
                // Return success response with file info
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody(
                    "{\"success\":true,"
                    "\"filename\":\"" + filename + "\","
                    "\"originalName\":\"" + file.getFileName() + "\","
                    "\"path\":\"" + filepath + "\","
                    "\"size\":" + std::to_string(file.fileLength()) + "}"
                );
                callback(resp);
            },
            {drogon::Post}
        );
        
        // Configure server
        drogon::app()
            .setLogPath("./")
            .setLogLevel(trantor::Logger::kInfo)
            .addListener("0.0.0.0", DEFAULT_PORT)
            .setThreadNum(DEFAULT_THREAD_NUM)
            .setDocumentRoot("./src/web_server/web")
            .setUploadPath(UPLOAD_PATH)
            .run();
        
        return 0;
    } catch (const std::exception& e) {
        spdlog::critical("Error starting server: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::critical("Unknown error occurred starting server");
        return 1;
    }
}