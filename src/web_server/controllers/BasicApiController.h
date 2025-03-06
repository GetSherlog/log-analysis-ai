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

// Include Folly libraries for thread safety
#include <folly/Synchronized.h>
#include <folly/container/F14Map.h>

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
        uploadProgress_.withWLock([&](auto& progressMap) {
            if (progressMap.find(upload_id) == progressMap.end()) {
                // Create a new progress entry
                progressMap[upload_id] = {0, 0, 0, "initializing", "Upload initialized"};
            }
        });
        
        // Create a proper SSE response with correct headers
        auto resp = drogon::HttpResponse::newAsyncStreamResponse([this, upload_id](drogon::ResponseStreamPtr stream) {
            // Check if we already have progress information
            bool hasProgress = uploadProgress_.withRLock([&](const auto& progressMap) {
                auto it = progressMap.find(upload_id);
                if (it != progressMap.end()) {
                    // Send the current progress immediately
                    sendProgressEvent(stream, upload_id);
                    return true;
                }
                return false;
            });
            
            // Store stream for later updates - need to use std::move since it's a unique_ptr
            activeStreams_.withWLock([&](auto& streamsMap) {
                streamsMap[upload_id] = std::move(stream);
            });
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
        uploadProgress_.withWLock([&](auto& progressMap) {
            spdlog::info("Initializing progress for upload ID: {}", upload_id);
            progressMap[upload_id] = {0, 0, 0, "initializing", "Upload initialized"};
            spdlog::info("Progress map size after adding upload ID {}: {}", upload_id, progressMap.size());
        });
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
        
        activeStreams_.withWLock([&](auto& streamsMap) {
            auto it = streamsMap.find(upload_id);
            if (it != streamsMap.end()) {
                try {
                    std::string finalMessage = "event: close\ndata: {\"message\":\"Stream closed\"}\n\n";
                    it->second->send(finalMessage);
                } catch (const std::exception& e) {
                    spdlog::error("Error sending final message before closing stream: {}", e.what());
                }
                
                // Remove the stream
                streamsMap.erase(it);
            }
        });
    }

    // Add a method to keep streams active for completed uploads
    void initStreamCleanupTimer() {
        spdlog::info("Initializing stream cleanup timer");
        // Run a timer every 2 minutes to clean up completed upload streams that have been open too long
        auto timerCallback = [this]() {
            spdlog::info("Stream cleanup timer triggered");
            
            try {
                // Current time
                auto now = std::chrono::system_clock::now();
                auto nowMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                
                // Using synchronized access - no explicit locks needed
                auto uploadProgressRLock = uploadProgress_.rlock();
                
                // Using withWLock for atomic operations on multiple containers
                auto removedCount = completionTimestamps_.withWLock([&](auto& timestamps) {
                    int count = 0;
                    // Check all upload progresses that are complete
                    for (const auto& [uploadId, progress] : *uploadProgressRLock) {
                        spdlog::info("Checking upload ID: {} with status: {}", uploadId, progress.status);
                        
                        if (progress.status == "complete" || progress.status == "error") {
                            // Check if we have a completion timestamp for this upload
                            if (timestamps.find(uploadId) == timestamps.end()) {
                                // First time seeing this completed upload, save the timestamp
                                spdlog::info("First time seeing completed upload ID: {}, saving timestamp", uploadId);
                                timestamps[uploadId] = nowMillis;
                            } else {
                                // This upload was already completed, check how long ago
                                int64_t completedAt = timestamps[uploadId];
                                int64_t deltaMillis = nowMillis - completedAt;
                                int64_t deltaMinutes = deltaMillis / (1000 * 60);
                                
                                spdlog::info("Upload ID: {} has been complete for {} minutes", uploadId, deltaMinutes);
                                
                                // If it's been more than 5 minutes, we can remove the stream
                                if (deltaMinutes >= 5) {
                                    // Check if we have an active stream for this upload
                                    bool hasStream = false;
                                    
                                    // Use atomic operation on activeStreams_
                                    activeStreams_.withWLock([&](auto& streams) {
                                        auto streamIt = streams.find(uploadId);
                                        if (streamIt != streams.end()) {
                                            hasStream = true;
                                            try {
                                                spdlog::info("Closing stream for upload ID: {} after inactivity", uploadId);
                                                std::string finalMessage = "event: close\ndata: {\"message\":\"Stream closed due to inactivity\"}\n\n";
                                                streamIt->second->send(finalMessage);
                                            } catch (const std::exception& e) {
                                                spdlog::error("Error sending final message before closing stream: {}", e.what());
                                            }
                                            
                                            // Remove the stream
                                            streams.erase(streamIt);
                                        }
                                    });
                                    
                                    count++;
                                }
                            }
                        }
                    }
                    return count;
                });
                
                spdlog::info("Cleanup complete, removed {} stale streams", removedCount);
            } catch (const std::exception& e) {
                spdlog::error("Error during stream cleanup: {}", e.what());
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
            // Check if this upload ID exists - using folly::Synchronized read lock
            bool uploadExists = false;
            UploadProgress progress;
            
            // Using read-only lock since we're only checking status
            uploadProgress_.withRLock([&](const auto& progressMap) {
                spdlog::info("Checking if upload ID: {} exists in progress map", upload_id);
                auto it = progressMap.find(upload_id);
                if (it != progressMap.end()) {
                    uploadExists = true;
                    progress = it->second;  // Copy the progress data
                    spdlog::info("Upload ID: {} found in progress map with status: {}", upload_id, progress.status);
                }
            });
            
            if (uploadExists) {
                // Return the current status
                try {
                    // Create JSON response with upload progress information
                    Json::Value root;
                    root["upload_id"] = upload_id;
                    root["status"] = progress.status;
                    root["message"] = progress.message;
                    root["total_bytes"] = Json::UInt64(progress.totalBytes);
                    root["uploaded_bytes"] = Json::UInt64(progress.uploadedBytes);
                    root["progress_percent"] = progress.progressPercent;
                    
                    Json::FastWriter writer;
                    std::string response = writer.write(root);
                    
                    resp->setBody(response);
                    spdlog::info("About to execute callback for upload ID: {}", upload_id);
                    callback(resp);
                    spdlog::info("Callback executed for upload ID: {}", upload_id);
                } catch (const std::exception& ex) {
                    spdlog::error("Error creating JSON response for upload ID: {}: {}", upload_id, ex.what());
                    
                    // Create a simple error response
                    Json::Value errorRoot;
                    errorRoot["error"] = "Internal server error";
                    errorRoot["message"] = "Failed to create status response";
                    
                    Json::FastWriter writer;
                    resp->setBody(writer.write(errorRoot));
                    resp->setStatusCode(drogon::k500InternalServerError);
                    
                    callback(resp);
                }
            } else {
                // Upload ID not found
                spdlog::warn("Upload ID: {} not found in progress map", upload_id);
                
                Json::Value errorRoot;
                errorRoot["error"] = "Not found";
                errorRoot["message"] = "Upload ID not found";
                
                Json::FastWriter writer;
                resp->setBody(writer.write(errorRoot));
                resp->setStatusCode(drogon::k404NotFound);
                
                spdlog::info("About to execute callback for upload ID: {}", upload_id);
                callback(resp);
                spdlog::info("Callback executed for upload ID: {}", upload_id);
            }
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
    // Use Folly's thread-safe containers and synchronization primitives
    // Map to store upload progress data
    using UploadProgressMap = folly::F14FastMap<std::string, UploadProgress>;
    folly::Synchronized<UploadProgressMap> uploadProgress_;
    
    // Map to store active streams
    using StreamMap = folly::F14FastMap<std::string, drogon::ResponseStreamPtr>;
    folly::Synchronized<StreamMap> activeStreams_;
    
    // Map to track when uploads were completed
    using TimestampMap = folly::F14FastMap<std::string, int64_t>;
    folly::Synchronized<TimestampMap> completionTimestamps_;
    
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
        
        // Calculate progress percentage
        int progressPercent = (totalBytes > 0) ? static_cast<int>((static_cast<double>(uploadedBytes) / totalBytes) * 100) : 0;
        
        // Update progress in the map using folly::Synchronized
        uploadProgress_.withWLock([&](auto& progressMap) {
            spdlog::info("Updating progress for upload ID: {}", upload_id);
            
            // Update or create the progress entry
            auto& entry = progressMap[upload_id];
            entry.totalBytes = totalBytes;
            entry.uploadedBytes = uploadedBytes;
            entry.progressPercent = progressPercent;
            entry.status = status;
            entry.message = message;
            
            spdlog::info("Progress updated for upload ID: {} - Progress: {}%", upload_id, progressPercent);
        });
        
        // If status is complete or error, update completion timestamp
        if (status == "complete" || status == "error") {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
                
            completionTimestamps_.withWLock([&](auto& timestamps) {
                timestamps[upload_id] = timestamp;
                spdlog::info("Upload ID: {} marked as {} at timestamp: {}", upload_id, status, timestamp);
            });
        }
        
        // Check if we have an active stream for this upload
        activeStreams_.withRLock([&](const auto& streams) {
            auto it = streams.find(upload_id);
            if (it != streams.end()) {
                try {
                    sendProgressEvent(it->second, upload_id);
                    
                    // If status is error, we can close the stream immediately (must be done outside this read lock)
                    if (status == "error") {
                        // Schedule closing this stream, can't modify during read lock
                        drogon::app().getLoop()->runInLoop([this, upload_id]() {
                            activeStreams_.withWLock([&](auto& mutableStreams) {
                                auto streamIt = mutableStreams.find(upload_id);
                                if (streamIt != mutableStreams.end()) {
                                    std::string finalMessage = "event: close\ndata: {\"message\":\"Stream closed due to error\"}\n\n";
                                    streamIt->second->send(finalMessage);
                                    mutableStreams.erase(streamIt);
                                }
                            });
                        });
                    }
                    // For "complete" status, we'll keep the stream open and let the cleanup timer handle it
                } catch (const std::exception& e) {
                    spdlog::error("Error sending progress event to stream: {}", e.what());
                }
            }
        });
    }
    
    /**
     * @brief Send progress event for a specific upload
     */
    void sendProgressEvent(const drogon::ResponseStreamPtr& stream, const std::string& upload_id) {
        UploadProgress progress;
        bool found = uploadProgress_.withRLock([&](const auto& progressMap) {
            auto it = progressMap.find(upload_id);
            if (it == progressMap.end()) {
                spdlog::info("No progress data found for upload ID: {}", upload_id);
                return false;
            }
            progress = it->second;
            return true;
        });
        
        if (!found) return;
        
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
        bool found = uploadProgress_.withRLock([&](const auto& progressMap) {
            auto it = progressMap.find(upload_id);
            if (it == progressMap.end()) {
                return false;
            }
            progress = it->second;
            return true;
        });
        
        if (!found) return;
        
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