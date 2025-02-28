/**
 * @file LogAIServer.cpp
 * @brief High-performance Web server exposing LogAI-CPP functionality using Drogon framework
 */

#include <drogon/drogon.h>
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <filesystem>

// LogAI includes
#include "file_data_loader.h"
#include "data_loader_config.h"
#include "preprocessor.h"
#include "drain_parser.h"
#include "feature_extractor.h"
#include "label_encoder.h"
#include "logbert_vectorizer.h"
#include "one_class_svm.h"
#include "dbscan_clustering.h"
#include "dbscan_clustering_kdtree.h"

// Web server controllers
#include "controllers/LogParserController.h"
#include "controllers/AnomalyDetectionController.h"

using namespace logai::web;
namespace fs = std::filesystem;

// Server config constants
const int DEFAULT_PORT = 8080;
const int DEFAULT_THREAD_NUM = 16;
const std::string UPLOAD_PATH = "./uploads";

int main(int argc, char* argv[]) {
    // Process command-line arguments for port and thread count
    int port = DEFAULT_PORT;
    int threadNum = DEFAULT_THREAD_NUM;
    
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    if (argc > 2) {
        threadNum = std::stoi(argv[2]);
    }
    
    std::cout << "Starting LogAI-CPP Web Server on port " << port 
              << " with " << threadNum << " threads..." << std::endl;
    
    // Create upload directory if it doesn't exist
    if (!fs::exists(UPLOAD_PATH)) {
        fs::create_directory(UPLOAD_PATH);
    }
    
    // Configure CORS for the API
    auto corsConfig = std::make_shared<drogon::plugins::CorsConfig>();
    corsConfig->corsMethods = "GET,POST,OPTIONS";
    corsConfig->corsHeaders = "Origin, Content-Type, Accept";
    corsConfig->corsOrigins = {"*"};  // In production, restrict this to specific domains
    corsConfig->corsExposeHeaders = "Authorization";
    corsConfig->corsAllowCredentials = false;
    corsConfig->corsMaxAge = 86400;  // 24 hours
    
    // Initialize Drogon server
    drogon::app()
        .setLogPath("./")
        .setLogLevel(trantor::Logger::kInfo)
        .addListener("0.0.0.0", port)
        .setThreadNum(threadNum)
        .enableRunAsDaemon()
        .enableServerHeader(false)
        .setIdleConnectionTimeout(60)
        .setMaxConnectionNum(10000)
        .setMaxConnectionNumPerIP(0)
        .setDocumentRoot("./web")
        .setUploadPath(UPLOAD_PATH)
        .registerSyncAdvice([](const drogon::HttpRequestPtr& req) {
            req->addHeader("Server", "LogAI-CPP Server");
            return drogon::HttpResponsePtr();
        })
        // Register builtin controllers
        .registerHandler("/health",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"status\":\"ok\",\"version\":\"0.1.0\"}");
                callback(resp);
            },
            {drogon::Get}
        )
        .registerHandler("/api",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"name\":\"LogAI-CPP API\",\"version\":\"0.1.0\",\"status\":\"running\"}");
                callback(resp);
            },
            {drogon::Get}
        )
        // File upload handler for the anomaly detection frontend
        .registerHandler("/api/upload",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                auto resp = drogon::HttpResponse::newHttpJsonResponse({});
                
                // Check if this is a multipart/form-data request
                if (req->getContentType() != drogon::CT_MULTIPART_FORM_DATA) {
                    resp = drogon::HttpResponse::newHttpJsonResponse({
                        {"error", true},
                        {"message", "Expecting multipart/form-data request"}
                    });
                    resp->setStatusCode(drogon::k400BadRequest);
                    callback(resp);
                    return;
                }
                
                // Get the file from the request
                auto fileUploads = req->getFiles();
                if (fileUploads.empty() || !fileUploads["file"]) {
                    resp = drogon::HttpResponse::newHttpJsonResponse({
                        {"error", true},
                        {"message", "No file found in request"}
                    });
                    resp->setStatusCode(drogon::k400BadRequest);
                    callback(resp);
                    return;
                }
                
                auto& file = fileUploads["file"];
                
                // Save the file to the upload directory with a unique name
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();
                
                std::string filename = std::to_string(timestamp) + "_" + file->getFileName();
                std::string filePath = UPLOAD_PATH + "/" + filename;
                
                // Save the file
                file->saveAs(filePath);
                
                // Return the file information
                resp = drogon::HttpResponse::newHttpJsonResponse({
                    {"success", true},
                    {"filename", filename},
                    {"originalName", file->getFileName()},
                    {"path", filePath},
                    {"size", file->fileLength()}
                });
                
                callback(resp);
            },
            {drogon::Post}
        )
        // Configure CORS
        .registerPlugin<drogon::plugins::Cors>(corsConfig)
        // Run the server
        .run();
    
    return 0;
} 