#pragma once

#include "ApiController.h"
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/MultiPartParser.h>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace logai {
namespace web {

/**
 * @class BasicApiController
 * @brief Controller for basic API endpoints like health check, info, and file upload
 */
class BasicApiController : public drogon::HttpController<BasicApiController>, public ApiController {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(BasicApiController::health, "/health", drogon::Get);
    ADD_METHOD_TO(BasicApiController::info, "/api", drogon::Get);
    ADD_METHOD_TO(BasicApiController::upload, "/api/upload", drogon::Post);
    METHOD_LIST_END

    BasicApiController() = default;
    ~BasicApiController() = default;

    /**
     * @brief Health check endpoint
     */
    void health(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        spdlog::debug("Health check request received");
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody("{\"status\":\"ok\"}");
        callback(resp);
    }

    /**
     * @brief API info endpoint
     */
    void info(const drogon::HttpRequestPtr& req,
             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        spdlog::debug("API info request received");
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody("{\"name\":\"LogAI-CPP API\",\"version\":\"0.1.0\",\"status\":\"running\"}");
        callback(resp);
    }

    /**
     * @brief File upload endpoint
     */
    void upload(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        spdlog::debug("File upload request received");
        auto resp = drogon::HttpResponse::newHttpResponse();
        
        // Parse multipart form data using MultiPartParser
        drogon::MultiPartParser fileUpload;
        if (fileUpload.parse(req) != 0) {
            spdlog::error("Failed to parse multipart form data");
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody("{\"error\":true,\"message\":\"Failed to parse multipart form data\"}");
            callback(resp);
            return;
        }
        
        const auto &files = fileUpload.getFiles();
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
    }
};

} // namespace web
} // namespace logai 