#include "AnomalyDetectionController.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <Eigen/Dense>
#include "regex_parser.h"
#include "data_loader_config.h"
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using MatrixXd = Eigen::MatrixXd;
using VectorXd = Eigen::VectorXd;

namespace logai {
namespace web {

AnomalyDetectionController::AnomalyDetectionController() {
    // Initialize feature extractor with default config
    FeatureExtractorConfig feConfig;
    featureExtractor_ = std::make_unique<FeatureExtractor>(feConfig);
    
    spdlog::info("AnomalyDetectionController initialized with default configuration");
    
    // Initialize other components
    labelEncoder_ = std::make_unique<LabelEncoder>();
    
    // Initialize LogBERTVectorizer with default config
    LogBERTVectorizerConfig lbConfig;
    logbertVectorizer_ = std::make_unique<LogBERTVectorizer>(lbConfig);
    
    // Initialize OneClassSVMDetector with default params
    OneClassSVMParams ocsvmParams;
    ocsvmParams.kernel = "rbf";
    ocsvmParams.nu = 0.1;
    ocsvmParams.gamma = "scale"; // Use "scale" instead of a fixed value
    oneClassSvm_ = std::make_unique<OneClassSVMDetector>(ocsvmParams);
    
    // Initialize DBSCAN with default params
    DbScanParams dbParams;
    dbParams.eps = 0.5;
    dbParams.min_samples = 5;
    dbscan_ = std::make_unique<DbScanClustering>(dbParams);
    
    // Initialize DBSCAN KDTree with default params
    DbScanKDTreeParams kdParams;
    kdParams.eps = 0.5;
    kdParams.min_samples = 5;
    dbscanKdtree_ = std::make_unique<DbScanClusteringKDTree>(kdParams);
    
    spdlog::debug("All anomaly detection components initialized");
}

void AnomalyDetectionController::extractFeatures(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    spdlog::info("Starting feature extraction request processing");
    
    // Parse request JSON
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        spdlog::error("Failed to parse JSON request body");
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("logLines") || !requestJson["logLines"].is_array()) {
        spdlog::error("Request validation failed: missing or invalid logLines array");
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
        spdlog::error("No valid log lines found in request");
        callback(createErrorResponse("No valid log lines provided"));
        return;
    }
    
    spdlog::info("Received {} log lines for processing", logLines.size());
    
    // Check if we need to update the feature extractor config
    bool configUpdated = false;
    FeatureExtractorConfig newConfig;
    
    // Process group_by_category
    if (requestJson.contains("groupByCategory") && requestJson["groupByCategory"].is_array()) {
        configUpdated = true;
        newConfig.group_by_category.clear();
        for (const auto& category : requestJson["groupByCategory"]) {
            if (category.is_string()) {
                newConfig.group_by_category.push_back(category.get<std::string>());
            }
        }
        spdlog::debug("Updated group_by_category with {} categories", newConfig.group_by_category.size());
    }
    
    // Process group_by_time
    if (requestJson.contains("groupByTime") && requestJson["groupByTime"].is_string()) {
        configUpdated = true;
        newConfig.group_by_time = requestJson["groupByTime"].get<std::string>();
        spdlog::debug("Updated group_by_time to: {}", newConfig.group_by_time);
    }
    
    // Process sliding_window
    if (requestJson.contains("slidingWindow") && requestJson["slidingWindow"].is_number_integer()) {
        configUpdated = true;
        newConfig.sliding_window = requestJson["slidingWindow"].get<int>();
        spdlog::debug("Updated sliding_window to: {}", newConfig.sliding_window);
    }
    
    // Process steps
    if (requestJson.contains("steps") && requestJson["steps"].is_number_integer()) {
        configUpdated = true;
        newConfig.steps = requestJson["steps"].get<int>();
        spdlog::debug("Updated steps to: {}", newConfig.steps);
    }
    
    // Process max_feature_len
    if (requestJson.contains("maxFeatureLength") && requestJson["maxFeatureLength"].is_number_integer()) {
        configUpdated = true;
        newConfig.max_feature_len = requestJson["maxFeatureLength"].get<int>();
        spdlog::debug("Updated max_feature_len to: {}", newConfig.max_feature_len);
    }
    
    // Update feature extractor if config changed
    if (configUpdated) {
        spdlog::info("Updating feature extractor with new configuration");
        featureExtractor_ = std::make_unique<FeatureExtractor>(newConfig);
    }
    
    // Parse log lines into LogRecordObjects
    std::vector<LogRecordObject> logRecords;
    logRecords.reserve(logLines.size());
    
    // Process timestamp format if provided
    std::string timestampFormat = "%Y-%m-%d %H:%M:%S";
    if (requestJson.contains("timestampFormat") && requestJson["timestampFormat"].is_string()) {
        timestampFormat = requestJson["timestampFormat"].get<std::string>();
        spdlog::debug("Using custom timestamp format: {}", timestampFormat);
    }
    
    // Create parser config
    DataLoaderConfig parserConfig;
    parserConfig.datetime_format = timestampFormat;
    
    // Set regex pattern - either use default or provided pattern
    std::string pattern = "(.*)";  // Default pattern treats whole line as body
    if (requestJson.contains("regexPattern") && requestJson["regexPattern"].is_string()) {
        pattern = requestJson["regexPattern"].get<std::string>();
        spdlog::debug("Using custom regex pattern: {}", pattern);
    }
    
    // Create a regex parser for the log lines
    RegexParser parser(parserConfig, pattern);
    
    size_t parseFailures = 0;
    for (const auto& line : logLines) {
        try {
            logRecords.push_back(parser.parse_line(line));
        } catch (const std::exception& e) {
            parseFailures++;
            spdlog::warn("Failed to parse log line: {}", e.what());
            continue;
        }
    }
    
    if (parseFailures > 0) {
        spdlog::warn("Failed to parse {} out of {} log lines", parseFailures, logLines.size());
    }
    
    if (logRecords.empty()) {
        spdlog::error("No log records created after parsing");
        callback(createErrorResponse("Failed to parse any log lines"));
        return;
    }
    
    spdlog::info("Successfully parsed {} log records", logRecords.size());
    
    // Determine which extraction method to use
    std::string method = "counter_vector";  // Default method
    if (requestJson.contains("method") && requestJson["method"].is_string()) {
        method = requestJson["method"].get<std::string>();
    }
    spdlog::info("Using feature extraction method: {}", method);
    
    FeatureExtractionResult extractionResult;
    
    try {
        if (method == "feature_vector") {
            spdlog::debug("Attempting feature vector extraction");
            // For feature_vector, we need log vectors
            std::shared_ptr<arrow::Table> logVectors;
            
            // Check if log vectors are provided in the request
            if (requestJson.contains("logVectors") && requestJson["logVectors"].is_array()) {
                spdlog::error("Log vectors from request not implemented yet");
                callback(createErrorResponse("Log vectors from request not implemented yet"));
                return;
            } else {
                spdlog::info("No log vectors provided, falling back to counter_vector");
                extractionResult = featureExtractor_->convert_to_counter_vector(logRecords);
            }
        } else if (method == "sequence") {
            spdlog::debug("Performing sequence extraction");
            extractionResult = featureExtractor_->convert_to_sequence(logRecords);
        } else {
            spdlog::debug("Performing counter vector extraction");
            extractionResult = featureExtractor_->convert_to_counter_vector(logRecords);
        }
    } catch (const std::exception& e) {
        spdlog::error("Feature extraction failed: {}", e.what());
        callback(createErrorResponse("Feature extraction failed: " + std::string(e.what())));
        return;
    }
    
    spdlog::info("Feature extraction completed with {} groups", extractionResult.event_indices.size());
    
    // Prepare response JSON
    json response;
    response["method"] = method;
    response["groups"] = json::array();
    
    // Add each group to the response
    for (size_t i = 0; i < extractionResult.event_indices.size(); ++i) {
        json group;
        
        // Add group identifiers
        group["groupIdentifiers"] = extractionResult.group_identifiers[i];
        
        // Add event indices
        group["eventIndices"] = extractionResult.event_indices[i];
        
        // Add count if available
        if (i < extractionResult.counts.size()) {
            group["count"] = extractionResult.counts[i];
        }
        
        // Add sequence if available
        if (method == "sequence" && i < extractionResult.sequences.size()) {
            group["sequence"] = extractionResult.sequences[i];
        }
        
        // Add feature vector if available
        if (method == "feature_vector" && extractionResult.feature_vectors && 
            i < static_cast<size_t>(extractionResult.feature_vectors->num_rows())) {
            
            json featureVector = json::array();
            
            try {
                // Process each column in the feature vector
                for (int col = 0; col < extractionResult.feature_vectors->num_columns(); ++col) {
                    const auto& column = extractionResult.feature_vectors->column(col);
                    
                    if (column->type()->id() == arrow::Type::DOUBLE) {
                        auto double_array = std::static_pointer_cast<arrow::DoubleArray>(column->chunk(0));
                        if (!double_array->IsNull(i)) {
                            featureVector.push_back(double_array->Value(i));
                        } else {
                            featureVector.push_back(0.0);  // Default value for null
                        }
                    } else if (column->type()->id() == arrow::Type::INT64) {
                        auto int_array = std::static_pointer_cast<arrow::Int64Array>(column->chunk(0));
                        if (!int_array->IsNull(i)) {
                            featureVector.push_back(static_cast<double>(int_array->Value(i)));
                        } else {
                            featureVector.push_back(0.0);  // Default value for null
                        }
                    } else {
                        // For other types, add a placeholder
                        featureVector.push_back(0.0);
                    }
                }
                
                group["featureVector"] = featureVector;
            } catch (const std::exception& e) {
                spdlog::error("Failed to process feature vector for group {}: {}", i, e.what());
            }
        }
        
        response["groups"].push_back(group);
    }
    
    // Add summary statistics
    response["totalGroups"] = extractionResult.event_indices.size();
    response["totalEvents"] = logRecords.size();
    
    spdlog::info("Response prepared with {} groups and {} total events", 
                 extractionResult.event_indices.size(), logRecords.size());
    
    callback(createJsonResponse(response));
}

void AnomalyDetectionController::vectorizeLogbert(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("logMessages") || !requestJson["logMessages"].is_array()) {
        callback(createErrorResponse("Request must contain 'logMessages' array"));
        return;
    }
    
    // Extract log messages
    std::vector<std::string> logMessages;
    for (const auto& message : requestJson["logMessages"]) {
        if (message.is_string()) {
            logMessages.push_back(message.get<std::string>());
        }
    }
    
    if (logMessages.empty()) {
        callback(createErrorResponse("No valid log messages provided"));
        return;
    }
    
    // Optional configuration - update the config if necessary
    if (requestJson.contains("maxSequenceLength") && requestJson["maxSequenceLength"].is_number_integer()) {
        // Create new config with updated max length
        LogBERTVectorizerConfig newConfig;
        newConfig.max_token_len = requestJson["maxSequenceLength"].get<int>();
        
        // Recreate the vectorizer with new config
        logbertVectorizer_ = std::make_unique<LogBERTVectorizer>(newConfig);
    }
    
    // Vectorize messages
    std::vector<std::vector<int>> tokens = logbertVectorizer_->transform(logMessages);
    
    // Convert to matrix for consistent API
    Eigen::MatrixXd vectors(tokens.size(), tokens[0].size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        for (size_t j = 0; j < tokens[i].size(); ++j) {
            vectors(i, j) = static_cast<double>(tokens[i][j]);
        }
    }
    
    // Prepare response
    json response;
    response["vectors"] = json::array();
    
    for (size_t i = 0; i < vectors.rows(); ++i) {
        json vectorJson = json::array();
        for (size_t j = 0; j < vectors.cols(); ++j) {
            vectorJson.push_back(vectors(i, j));
        }
        response["vectors"].push_back(vectorJson);
    }
    
    response["totalVectors"] = vectors.rows();
    response["vectorDimension"] = vectors.cols();
    
    callback(createJsonResponse(response));
}

void AnomalyDetectionController::detectAnomaliesOcSvm(const drogon::HttpRequestPtr& req,
                                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }

    if (!requestJson.contains("featureVectors") || !requestJson["featureVectors"].is_array()) {
        callback(createErrorResponse("Request must contain 'featureVectors' array"));
        return;
    }

    auto featureVectors = requestJson["featureVectors"].get<std::vector<std::vector<double>>>();
    if (featureVectors.empty()) {
        callback(createErrorResponse("No feature vectors provided"));
        return;
    }

    size_t dimension = featureVectors[0].size();
    for (const auto& vec : featureVectors) {
        if (vec.size() != dimension) {
            callback(createErrorResponse("All feature vectors must have the same dimension"));
            return;
        }
        for (const auto& value : vec) {
            if (!std::isfinite(value)) {
                callback(createErrorResponse("Feature values must be numeric"));
                return;
            }
        }
    }

    Eigen::MatrixXd featureMatrix(featureVectors.size(), dimension);
    for (size_t i = 0; i < featureVectors.size(); ++i) {
        for (size_t j = 0; j < dimension; ++j) {
            featureMatrix(i, j) = featureVectors[i][j];
        }
    }

    // Create new params if configuration is updated
    OneClassSVMParams params;
    params.kernel = "rbf"; // default
    params.nu = 0.1;      // default
    params.gamma = "scale"; // default
    
    if (requestJson.contains("kernel")) {
        params.kernel = requestJson["kernel"].get<std::string>();
    }

    if (requestJson.contains("nu")) {
        params.nu = requestJson["nu"].get<double>();
    }

    if (requestJson.contains("gamma")) {
        if (requestJson["gamma"].is_number()) {
            // If numeric value, convert to string
            params.gamma = std::to_string(requestJson["gamma"].get<double>());
        } else {
            params.gamma = requestJson["gamma"].get<std::string>();
        }
    }
    
    // Recreate the detector with updated params
    oneClassSvm_ = std::make_unique<OneClassSVMDetector>(params);

    // Fit and predict
    oneClassSvm_->fit(featureMatrix);
    Eigen::VectorXd predictions = oneClassSvm_->predict(featureMatrix);

    json response;
    response["predictions"] = std::vector<int>();
    for (Eigen::Index i = 0; i < predictions.size(); ++i) {
        response["predictions"].push_back(predictions(i));
    }

    callback(createJsonResponse(response));
}

void AnomalyDetectionController::clusterDbscan(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    json requestJson;
    if (!parseJsonBody(req, requestJson)) {
        callback(createErrorResponse("Invalid JSON in request body"));
        return;
    }

    if (!requestJson.contains("featureVectors") || !requestJson["featureVectors"].is_array()) {
        callback(createErrorResponse("Request must contain 'featureVectors' array"));
        return;
    }

    auto featureVectors = requestJson["featureVectors"].get<std::vector<std::vector<double>>>();
    if (featureVectors.empty()) {
        callback(createErrorResponse("No feature vectors provided"));
        return;
    }

    size_t dimension = featureVectors[0].size();
    for (const auto& vec : featureVectors) {
        if (vec.size() != dimension) {
            callback(createErrorResponse("All feature vectors must have the same dimension"));
            return;
        }
        for (const auto& value : vec) {
            if (!std::isfinite(value)) {
                callback(createErrorResponse("Feature values must be numeric"));
                return;
            }
        }
    }

    // Convert Eigen::MatrixXd to vector<vector<float>>
    std::vector<std::vector<float>> featureVectorsFloat;
    featureVectorsFloat.reserve(featureVectors.size());
    for (const auto& vec : featureVectors) {
        std::vector<float> floatVec(vec.begin(), vec.end());
        featureVectorsFloat.push_back(std::move(floatVec));
    }

    bool useKdTree = requestJson.value("useKdTree", false);
    Eigen::VectorXd labels;

    if (useKdTree) {
        // Create new params if needed
        DbScanKDTreeParams kdParams;
        kdParams.eps = 0.5; // default
        kdParams.min_samples = 5; // default

        if (requestJson.contains("eps")) {
            kdParams.eps = requestJson["eps"].get<double>();
        }

        if (requestJson.contains("minSamples")) {
            kdParams.min_samples = requestJson["minSamples"].get<int>();
        }

        // Recreate with new params
        dbscanKdtree_ = std::make_unique<DbScanClusteringKDTree>(kdParams);
        
        // Fit and get labels
        dbscanKdtree_->fit(featureVectorsFloat);
        std::vector<int> labelVec = dbscanKdtree_->get_labels();
        
        // Convert to Eigen vector
        labels.resize(labelVec.size());
        for (size_t i = 0; i < labelVec.size(); ++i) {
            labels(i) = labelVec[i];
        }
    } else {
        // Create new params if needed
        DbScanParams dbParams;
        dbParams.eps = 0.5; // default
        dbParams.min_samples = 5; // default
        
        if (requestJson.contains("eps")) {
            dbParams.eps = requestJson["eps"].get<double>();
        }

        if (requestJson.contains("minSamples")) {
            dbParams.min_samples = requestJson["minSamples"].get<int>();
        }

        // Recreate with new params
        dbscan_ = std::make_unique<DbScanClustering>(dbParams);
        
        // Fit and get labels
        dbscan_->fit(featureVectorsFloat);
        std::vector<int> labelVec = dbscan_->get_labels();
        
        // Convert to Eigen vector
        labels.resize(labelVec.size());
        for (size_t i = 0; i < labelVec.size(); ++i) {
            labels(i) = labelVec[i];
        }
    }

    json response;
    response["labels"] = std::vector<int>();
    for (Eigen::Index i = 0; i < labels.size(); ++i) {
        response["labels"].push_back(labels(i));
    }

    callback(createJsonResponse(response));
}

} // namespace web
} // namespace logai 