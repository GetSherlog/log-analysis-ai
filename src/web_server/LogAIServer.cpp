/**
 * @file LogAIServer.cpp
 * @brief Very simple web server for LogAI-CPP with Drogon
 */

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>
#include <string>
#include <memory>
#include <filesystem>

// Include controllers
#include "controllers/AnomalyDetectionController.h"
#include "controllers/BasicApiController.h"

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
        
        // Configure server
        drogon::app()
            .setLogPath("./")
            .setLogLevel(trantor::Logger::kInfo)
            .addListener("0.0.0.0", DEFAULT_PORT)
            .setThreadNum(DEFAULT_THREAD_NUM)
            .setDocumentRoot("./src/web_server/web")
            .setUploadPath(UPLOAD_PATH)
            .setClientMaxBodySize(1024 * 1024 * 1024); // 1GB in bytes
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