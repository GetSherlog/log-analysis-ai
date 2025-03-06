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
    ADD_METHOD_TO(BasicApiController::uploadStatus, "/api/upload/status/{upload_id}", drogon::Get);
    METHOD_LIST_END

    BasicApiController() {
        spdlog::info("BasicApiController initialized");
        
        // Create uploads directory if it doesn't exist
        if (!fs::exists("./uploads")) {
            fs::create_directory("./uploads");
            spdlog::info("Created uploads directory");
        }
        
        // Initialize the stream cleanup timer
        initStreamCleanupTimer();
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
            spdlog::info("Attempting to acquire progressMutex_ lock for initializing upload ID: {}", upload_id);
            std::lock_guard<std::mutex> lock(progressMutex_);
            spdlog::info("Lock acquired for progressMutex_, initializing progress for upload ID: {}", upload_id);
            uploadProgress_[upload_id] = {0, 0, 0, "initializing", "Upload initialized"};
            spdlog::info("Progress map size after adding upload ID {}: {}", upload_id, uploadProgress_.size());
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
            // Don't close the stream immediately - frontend may not have connected yet
            // closeStream(upload_id);
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

    // Add a method to keep streams active for completed uploads
    void initStreamCleanupTimer() {
        spdlog::info("Initializing stream cleanup timer");
        // Run a timer every 2 minutes to clean up completed upload streams that have been open too long
        auto timerCallback = [this]() {
            spdlog::info("Stream cleanup timer triggered");
            
            try {
                spdlog::info("Attempting to acquire both mutex locks for cleanup");
                std::lock_guard<std::mutex> progressLock(progressMutex_);
                std::lock_guard<std::mutex> streamLock(streamsMutex_);
                spdlog::info("Locks acquired for cleanup, checking {} upload progress entries and {} completion timestamps", 
                            uploadProgress_.size(), completionTimestamps_.size());
                
                // Current time
                auto now = std::chrono::system_clock::now();
                auto nowMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                    
                // Check all upload progresses that are complete
                for (const auto& [uploadId, progress] : uploadProgress_) {
                    spdlog::info("Checking upload ID: {} with status: {}", uploadId, progress.status);
                    
                    if (progress.status == "complete" || progress.status == "error") {
                        // Check if we have a completion timestamp for this upload
                        if (completionTimestamps_.find(uploadId) == completionTimestamps_.end()) {
                            // First time seeing this completed upload, save the timestamp
                            spdlog::info("First time seeing completed upload ID: {}, saving timestamp", uploadId);
                            completionTimestamps_[uploadId] = nowMillis;
                        } else {
                            // Check if this completed upload has been open for more than 5 minutes (300000 ms)
                            auto timestampMillis = completionTimestamps_[uploadId];
                            auto elapsedMs = nowMillis - timestampMillis;
                            spdlog::info("Upload ID: {} has been completed for {} ms", uploadId, elapsedMs);
                            
                            if (elapsedMs > 300000) {
                                // Close stream if it's been open too long
                                spdlog::info("Cleaning up long-running completed upload stream for ID: {}", uploadId);
                                auto streamIter = activeStreams_.find(uploadId);
                                if (streamIter != activeStreams_.end()) {
                                    try {
                                        spdlog::info("Sending final message to stream for upload ID: {}", uploadId);
                                        std::string finalMessage = "event: close\ndata: {\"message\":\"Stream closed after timeout\"}\n\n";
                                        streamIter->second->send(finalMessage);
                                        activeStreams_.erase(streamIter);
                                        spdlog::info("Stream removed for upload ID: {}", uploadId);
                                    } catch (const std::exception& e) {
                                        spdlog::error("Error closing stale stream for upload ID: {}: {}", uploadId, e.what());
                                    }
                                } else {
                                    spdlog::info("No active stream found for upload ID: {}", uploadId);
                                }
                                
                                // Also remove the timestamp entry
                                spdlog::info("Removing timestamp entry for upload ID: {}", uploadId);
                                completionTimestamps_.erase(uploadId);
                                
                                // NOTE: We are NOT removing the upload progress entry itself
                                // This ensures status check requests can still get the final status
                                spdlog::info("Upload progress entry for ID: {} is kept for status checks", uploadId);
                            }
                        }
                    }
                }
                spdlog::info("Stream cleanup completed, remaining entries: progress={}, timestamps={}, streams={}", 
                            uploadProgress_.size(), completionTimestamps_.size(), activeStreams_.size());
            } catch (const std::exception& e) {
                spdlog::error("Exception in stream cleanup timer: {}", e.what());
            }
        };
        
        // Schedule the timer
        spdlog::info("Scheduling stream cleanup timer to run every 2 minutes");
        drogon::app().getLoop()->runEvery(2.0 * 60, timerCallback);
    }

    /**
     * @brief Get the status of an upload
     */
    void uploadStatus(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& upload_id) {
        spdlog::info("Status check requested for upload ID: {}", upload_id);
        
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        
        try {
            // Check if this upload ID exists
            {
                spdlog::info("Acquiring progressMutex_ lock for upload ID: {}", upload_id);
                std::lock_guard<std::mutex> lock(progressMutex_);
                spdlog::info("Lock acquired for progressMutex_, checking if upload ID: {} exists", upload_id);
                
                auto it = uploadProgress_.find(upload_id);
                if (it != uploadProgress_.end()) {
                    // Return the current status
                    spdlog::info("Upload ID: {} found in progress map with status: {}", upload_id, it->second.status);
                    const auto& progress = it->second;
                    
                    try {
                        std::string responseJson = 
                            "{\"progress\":" + std::to_string(progress.progressPercent) + 
                            ",\"status\":\"" + progress.status + 
                            "\",\"message\":\"" + progress.message + 
                            "\",\"totalBytes\":" + std::to_string(progress.totalBytes) + 
                            "\",\"uploadedBytes\":" + std::to_string(progress.uploadedBytes) + 
                            "}";
                        
                        spdlog::info("Response JSON constructed for upload ID: {}: {}", upload_id, responseJson);
                        resp->setBody(responseJson);
                        resp->setStatusCode(drogon::k200OK);
                    } catch (const std::exception& e) {
                        spdlog::error("Exception while constructing JSON response for upload ID: {}: {}", upload_id, e.what());
                        resp->setBody("{\"error\":true,\"message\":\"Internal server error while processing upload status\"}");
                        resp->setStatusCode(drogon::k500InternalServerError);
                    }
                } else {
                    // Upload ID not found
                    spdlog::warn("Upload ID: {} not found in progress map", upload_id);
                    resp->setBody("{\"error\":true,\"message\":\"Upload ID not found\"}");
                    resp->setStatusCode(drogon::k404NotFound);
                }
                spdlog::info("Releasing progressMutex_ lock for upload ID: {}", upload_id);
            }
            
            spdlog::info("About to execute callback for upload ID: {}", upload_id);
            callback(resp);
            spdlog::info("Callback executed for upload ID: {}", upload_id);
        } catch (const std::exception& e) {
            spdlog::error("Uncaught exception in uploadStatus for upload ID: {}: {}", upload_id, e.what());
            auto errorResp = drogon::HttpResponse::newHttpResponse();
            errorResp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            errorResp->setBody("{\"error\":true,\"message\":\"Server error processing status request\"}");
            errorResp->setStatusCode(drogon::k500InternalServerError);
            
            try {
                callback(errorResp);
                spdlog::info("Error response callback executed for upload ID: {}", upload_id);
            } catch (const std::exception& innerEx) {
                spdlog::error("Failed to execute error callback for upload ID: {}: {}", upload_id, innerEx.what());
            }
        }
    }

private:
    // Map to store upload progress data
    std::unordered_map<std::string, UploadProgress> uploadProgress_;
    std::mutex progressMutex_;
    
    // Map to store active streams
    std::mutex streamsMutex_;
    std::unordered_map<std::string, drogon::ResponseStreamPtr> activeStreams_;
    
    // Map to track when uploads were completed
    std::unordered_map<std::string, int64_t> completionTimestamps_;
    
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
        spdlog::info("Updating progress for upload ID: {} - Status: {}, Message: {}, Uploaded: {}/{} bytes", 
                    upload_id, status, message, uploadedBytes, totalBytes);
        
        // Update progress in the map
        {
            spdlog::info("Attempting to acquire progressMutex_ lock for updating upload ID: {}", upload_id);
            std::lock_guard<std::mutex> lock(progressMutex_);
            spdlog::info("Lock acquired for progressMutex_, updating progress for upload ID: {}", upload_id);
            
            auto it = uploadProgress_.find(upload_id);
            if (it == uploadProgress_.end()) {
                spdlog::warn("Update called for non-existent upload ID: {}, will create new entry", upload_id);
            }
            
            // Calculate progress percentage
            int progressPercent = 0;
            if (totalBytes > 0 && uploadedBytes <= totalBytes) {
                progressPercent = static_cast<int>((static_cast<double>(uploadedBytes) / totalBytes) * 100);
            }
            
            uploadProgress_[upload_id] = {totalBytes, uploadedBytes, progressPercent, status, message};
            spdlog::info("Progress updated for upload ID: {} - Progress: {}%", upload_id, progressPercent);
            
            // If the status is "complete" or "error", store the completion timestamp
            if (status == "complete" || status == "error") {
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();
                
                spdlog::info("Upload ID: {} marked as {} at timestamp: {}", upload_id, status, timestamp);
                completionTimestamps_[upload_id] = timestamp;
            }
        }
        
        // Check if we have an active stream for this upload
        {
            std::lock_guard<std::mutex> lock(streamsMutex_);
            auto it = activeStreams_.find(upload_id);
            if (it != activeStreams_.end()) {
                try {
                    sendProgressEvent(it->second, upload_id);
                    
                    // If status is error, we can close the stream immediately
                    if (status == "error") {
                        std::string finalMessage = "event: close\ndata: {\"message\":\"Stream closed due to error\"}\n\n";
                        it->second->send(finalMessage);
                        activeStreams_.erase(it);
                    }
                    // For "complete" status, we'll keep the stream open and let the cleanup timer handle it
                    // or let the client disconnect when it's done
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