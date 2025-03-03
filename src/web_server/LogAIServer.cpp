/**
 * @file LogAIServer.cpp
 * @brief Very simple web server for LogAI-CPP with Drogon
 */

#include <drogon/drogon.h>
#include <iostream>
#include <string>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Server config constants
const int DEFAULT_PORT = 8080;
const int DEFAULT_THREAD_NUM = 2;
const std::string UPLOAD_PATH = "./uploads";

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
        std::cout << "Starting LogAI-CPP Web Server on port " << DEFAULT_PORT 
                  << " with " << DEFAULT_THREAD_NUM << " threads..." << std::endl;
        
        // Create upload directory if it doesn't exist
        if (!fs::exists(UPLOAD_PATH)) {
            fs::create_directory(UPLOAD_PATH);
        }
        
        // Create a simple health controller
        drogon::app().registerHandler(
            "/health",
            [](const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
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
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"name\":\"LogAI-CPP API\",\"version\":\"0.1.0\",\"status\":\"running\"}");
                callback(resp);
            }
        );
        
        // Register the CORS filter
        auto corsFilterPtr = std::make_shared<CorsFilter>();
        drogon::app().registerFilter(corsFilterPtr);
        
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
        std::cerr << "Error starting server: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred starting server" << std::endl;
        return 1;
    }
}