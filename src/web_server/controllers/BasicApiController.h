#pragma once

#include "ApiController.h"
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <chrono>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace logai {
namespace web {

// Structure to keep track of upload progress
struct UploadProgress {
    size_t totalBytes;
    size_t uploadedBytes;
    int progressPercent;
    std::string status; // "uploading", "processing", "complete", "error"
    std::string message;
};

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
    ADD_METHOD_TO(BasicApiController::progressEvents, "/api/upload/progress/{upload_id}", drogon::Get);
    METHOD_LIST_END

    BasicApiController() {
        // No cleanup timer needed
    }
    
    ~BasicApiController() {
        // No cleanup timer to invalidate
    }

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
     * @brief Server-Sent Events endpoint for upload progress
     */
    void progressEvents(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& upload_id) {
        spdlog::info("SSE connection requested for upload ID: {}", upload_id);
        
        // Check if this upload ID exists
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            if (uploadProgress_.find(upload_id) == uploadProgress_.end()) {
                // Create a new progress entry
                uploadProgress_[upload_id] = {0, 0, 0, "initializing", "Upload initialized"};
            }
        }
        
        // Create a proper SSE response with correct headers
        auto resp = drogon::HttpResponse::newAsyncStreamResponse([this, upload_id](drogon::ResponseStreamPtr stream) {
            // Check if we already have progress information
            {
                std::lock_guard<std::mutex> lock(progressMutex_);
                auto it = uploadProgress_.find(upload_id);
                if (it != uploadProgress_.end()) {
                    // Send the current progress immediately
                    sendProgressEvent(stream, upload_id);
                }
            }
            
            // Store stream for later updates - need to use std::move since it's a unique_ptr
            {
                std::lock_guard<std::mutex> lock(streamsMutex_);
                activeStreams_[upload_id] = std::move(stream);
            }
        });
        
        // Set SSE headers on the actual response
        resp->setContentTypeString("text/event-stream");
        resp->addHeader("Cache-Control", "no-cache");
        resp->addHeader("Connection", "keep-alive");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        
        callback(resp);
    }

    /**
     * @brief File upload endpoint
     */
    void upload(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        spdlog::info("File upload request received");
        auto resp = drogon::HttpResponse::newHttpResponse();
        
        // Generate an upload ID
        std::string upload_id = generateUploadId();
        spdlog::info("Generated upload ID: {}", upload_id);
        
        // Initialize progress tracking
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            uploadProgress_[upload_id] = {0, 0, 0, "initializing", "Upload initialized"};
        }
        spdlog::info("Upload progress initialized for ID: {}", upload_id);
        
        // Parse multipart form data using MultiPartParser
        drogon::MultiPartParser fileUpload;
        spdlog::info("Attempting to parse multipart form data for upload ID: {}", upload_id);
        if (fileUpload.parse(req) != 0) {
            spdlog::error("Failed to parse multipart form data for upload ID: {}", upload_id);
            updateProgress(upload_id, 0, 0, "error", "Failed to parse multipart form data");
            closeStream(upload_id);
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody("{\"error\":true,\"message\":\"Failed to parse multipart form data\",\"upload_id\":\"" + upload_id + "\"}");
            callback(resp);
            return;
        }
        
        spdlog::info("Successfully parsed multipart form data for upload ID: {}", upload_id);
        const auto &files = fileUpload.getFiles();
        spdlog::info("Found {} files in request for upload ID: {}", files.size(), upload_id);
        if (files.empty()) {
            spdlog::warn("No files found in upload request");
            updateProgress(upload_id, 0, 0, "error", "No files uploaded");
            closeStream(upload_id);
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody("{\"error\":true,\"message\":\"No files uploaded\",\"upload_id\":\"" + upload_id + "\"}");
            callback(resp);
            return;
        }
        
        // First file in the request
        const auto &file = files[0];
        size_t fileSize = file.fileLength();
        spdlog::info("Processing file: {} with size: {} bytes", file.getFileName(), fileSize);
        
        // Update progress with file size information
        updateProgress(upload_id, fileSize, 0, "uploading", "Starting upload");
        
        // Generate a timestamp for the filename
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        // Create a unique filename
        std::string filename = std::to_string(timestamp) + "_" + file.getFileName();
        std::string filepath = "./uploads/" + filename;
        spdlog::info("Generated filename for upload {}: {}", upload_id, filename);
        
        // Save the file
        try {
            // Update status to processing
            updateProgress(upload_id, fileSize, fileSize, "processing", "File received, processing...");
            spdlog::info("File received for upload {}, starting processing", upload_id);
            
            // Check if the file already exists
            if (fs::exists(filepath)) {
                spdlog::warn("File already exists at path: {}", filepath);
                // We'll continue and overwrite it
            }
            
            spdlog::info("Attempting to save file to: {} for upload ID: {}", filepath, upload_id);
            file.saveAs(filepath);
            spdlog::info("File saved successfully: {}", filepath);
            
            // Update status to complete
            updateProgress(upload_id, fileSize, fileSize, "complete", "Upload complete");
            closeStream(upload_id);
        } catch (const std::exception& e) {
            spdlog::error("Failed to save file {}: {}", filepath, e.what());
            updateProgress(upload_id, fileSize, 0, "error", std::string("Failed to save file: ") + e.what());
            closeStream(upload_id);
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody("{\"error\":true,\"message\":\"Failed to save uploaded file\",\"upload_id\":\"" + upload_id + "\"}");
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
            "\"size\":" + std::to_string(file.fileLength()) + ","
            "\"upload_id\":\"" + upload_id + "\"}"
        );
        
        try {
            spdlog::info("Sending success response for upload ID: {}", upload_id);
            callback(resp);
            spdlog::info("Response sent successfully for upload ID: {}", upload_id);
        } catch (const std::exception& e) {
            spdlog::error("Failed to send response for upload ID {}: {}", upload_id, e.what());
        } catch (...) {
            spdlog::error("Unknown error when sending response for upload ID: {}", upload_id);
        }
    }

    /**
     * @brief Close the stream for a specific upload ID
     */
    void closeStream(const std::string& upload_id) {
        spdlog::info("Closing stream for upload ID: {}", upload_id);
        std::lock_guard<std::mutex> lock(streamsMutex_);
        auto it = activeStreams_.find(upload_id);
        if (it != activeStreams_.end()) {
            try {
                std::string finalMessage = "event: close\ndata: {\"message\":\"Stream closed\"}\n\n";
                it->second->send(finalMessage);
            } catch (const std::exception& e) {
                spdlog::error("Error sending final message before closing stream: {}", e.what());
            }
            
            // Remove the stream
            activeStreams_.erase(it);
        }
    }

private:
    // Map to store upload progress data
    std::unordered_map<std::string, UploadProgress> uploadProgress_;
    std::mutex progressMutex_;
    
    // Map to store active streams
    std::mutex streamsMutex_;
    std::unordered_map<std::string, drogon::ResponseStreamPtr> activeStreams_;
    
    /**
     * @brief Generate a unique upload ID
     */
    std::string generateUploadId() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        return std::to_string(timestamp);
    }
    
    /**
     * @brief Update progress for a specific upload
     */
    void updateProgress(const std::string& upload_id, 
                        size_t totalBytes, 
                        size_t uploadedBytes, 
                        const std::string& status,
                        const std::string& message) {
        std::lock_guard<std::mutex> lock(progressMutex_);
        int percent = totalBytes > 0 ? (int)((uploadedBytes * 100) / totalBytes) : 0;
        uploadProgress_[upload_id] = {totalBytes, uploadedBytes, percent, status, message};
        spdlog::info("Progress update for {}: {}% - Status: {} - Message: {}", upload_id, percent, status, message);
        
        // Check if we have an active stream for this upload
        {
            std::lock_guard<std::mutex> lock(streamsMutex_);
            auto it = activeStreams_.find(upload_id);
            if (it != activeStreams_.end()) {
                try {
                    sendProgressEvent(it->second, upload_id);
                } catch (const std::exception& e) {
                    spdlog::error("Error sending progress event: {}", e.what());
                }
            }
        }
    }
    
    /**
     * @brief Send progress event for a specific upload
     */
    void sendProgressEvent(const drogon::ResponseStreamPtr& stream, const std::string& upload_id) {
        UploadProgress progress;
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            if (uploadProgress_.find(upload_id) == uploadProgress_.end()) {
                spdlog::info("No progress data found for upload ID: {}", upload_id);
                return;
            }
            progress = uploadProgress_[upload_id];
        }
        
        std::string eventData = "{\"progress\":" + std::to_string(progress.progressPercent) + 
                                ",\"status\":\"" + progress.status + 
                                "\",\"message\":\"" + progress.message + 
                                "\",\"totalBytes\":" + std::to_string(progress.totalBytes) + 
                                ",\"uploadedBytes\":" + std::to_string(progress.uploadedBytes) + "}";
        spdlog::info("Sending progress event for {}: {}% complete, status: {}", upload_id, progress.progressPercent, progress.status);
        
        // Create the SSE message
        std::string sseMessage = "event: progress\ndata: " + eventData + "\n\n";
        
        // Send it - must use the single-argument form
        stream->send(sseMessage);
    }
    
    /**
     * @brief Overload for HttpResponse version for compatibility
     */
    void sendProgressEvent(const drogon::HttpResponsePtr& resp, const std::string& upload_id) {
        UploadProgress progress;
        {
            std::lock_guard<std::mutex> lock(progressMutex_);
            if (uploadProgress_.find(upload_id) == uploadProgress_.end()) {
                return;
            }
            progress = uploadProgress_[upload_id];
        }
        
        std::string eventData = "{\"progress\":" + std::to_string(progress.progressPercent) + 
                                ",\"status\":\"" + progress.status + 
                                "\",\"message\":\"" + progress.message + 
                                "\",\"totalBytes\":" + std::to_string(progress.totalBytes) + 
                                ",\"uploadedBytes\":" + std::to_string(progress.uploadedBytes) + "}";
        
        resp->setBody("data: " + eventData + "\n\n");
    }
};

} // namespace web
} // namespace logai 