#include "AnomalyDetectionController.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <Eigen/Dense>
#include "regex_parser.h"
#include "data_loader_config.h"

using json = nlohmann::json;
using MatrixXd = Eigen::MatrixXd;
using VectorXd = Eigen::VectorXd;

namespace logai {
namespace web {

AnomalyDetectionController::AnomalyDetectionController() {
    // Initialize feature extractor with default config
    FeatureExtractorConfig feConfig;
    featureExtractor_ = std::make_unique<FeatureExtractor>(feConfig);
    
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
}

void AnomalyDetectionController::extractFeatures(
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
    }
    
    // Process group_by_time
    if (requestJson.contains("groupByTime") && requestJson["groupByTime"].is_string()) {
        configUpdated = true;
        newConfig.group_by_time = requestJson["groupByTime"].get<std::string>();
    }
    
    // Process sliding_window
    if (requestJson.contains("slidingWindow") && requestJson["slidingWindow"].is_number_integer()) {
        configUpdated = true;
        newConfig.sliding_window = requestJson["slidingWindow"].get<int>();
    }
    
    // Process steps
    if (requestJson.contains("steps") && requestJson["steps"].is_number_integer()) {
        configUpdated = true;
        newConfig.steps = requestJson["steps"].get<int>();
    }
    
    // Process max_feature_len
    if (requestJson.contains("maxFeatureLength") && requestJson["maxFeatureLength"].is_number_integer()) {
        configUpdated = true;
        newConfig.max_feature_len = requestJson["maxFeatureLength"].get<int>();
    }
    
    // Update feature extractor if config changed
    if (configUpdated) {
        featureExtractor_ = std::make_unique<FeatureExtractor>(newConfig);
    }
    
    // Parse log lines into LogRecordObjects
    std::vector<LogRecordObject> logRecords;
    logRecords.reserve(logLines.size());
    
    // Process timestamp format if provided
    std::string timestampFormat = "%Y-%m-%d %H:%M:%S";
    if (requestJson.contains("timestampFormat") && requestJson["timestampFormat"].is_string()) {
        timestampFormat = requestJson["timestampFormat"].get<std::string>();
    }
    
    // Create parser config
    DataLoaderConfig parserConfig;
    parserConfig.datetime_format = timestampFormat;
    
    // Set regex pattern - either use default or provided pattern
    std::string pattern = "(.*)";  // Default pattern treats whole line as body
    if (requestJson.contains("regexPattern") && requestJson["regexPattern"].is_string()) {
        pattern = requestJson["regexPattern"].get<std::string>();
    }
    
    // Create a regex parser for the log lines
    RegexParser parser(parserConfig, pattern);
    
    for (const auto& line : logLines) {
        try {
            logRecords.push_back(parser.parse_line(line));
        } catch (const std::exception& e) {
            // Skip lines that fail to parse
            continue;
        }
    }
    
    if (logRecords.empty()) {
        callback(createErrorResponse("Failed to parse any log lines"));
        return;
    }
    
    // Determine which extraction method to use
    std::string method = "counter_vector";  // Default method
    if (requestJson.contains("method") && requestJson["method"].is_string()) {
        method = requestJson["method"].get<std::string>();
    }
    
    FeatureExtractionResult extractionResult;
    
    if (method == "feature_vector") {
        // For feature_vector, we need log vectors
        // In a real implementation, these would come from a vectorizer
        // For now, we'll create a simple dummy table
        std::shared_ptr<arrow::Table> logVectors;
        
        // Check if log vectors are provided in the request
        if (requestJson.contains("logVectors") && requestJson["logVectors"].is_array()) {
            // Process log vectors from request
            // This is a simplified implementation
            // In a real app, you'd parse the vectors properly
            callback(createErrorResponse("Log vectors from request not implemented yet"));
            return;
        } else {
            // Use convert_to_counter_vector as fallback
            extractionResult = featureExtractor_->convert_to_counter_vector(logRecords);
        }
    } else if (method == "sequence") {
        // Use convert_to_sequence
        extractionResult = featureExtractor_->convert_to_sequence(logRecords);
    } else {
        // Default: use convert_to_counter_vector
        extractionResult = featureExtractor_->convert_to_counter_vector(logRecords);
    }
    
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
        
        response["groups"].push_back(group);
    }
    
    // Add summary statistics
    response["totalGroups"] = extractionResult.event_indices.size();
    response["totalEvents"] = logRecords.size();
    
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