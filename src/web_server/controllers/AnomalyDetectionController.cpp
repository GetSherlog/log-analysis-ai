#include "AnomalyDetectionController.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <Eigen/Dense>

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
    
    // Extract features
    auto features = featureExtractor_->transform(logLines);
    
    // Prepare response
    json response;
    response["features"] = json::array();
    
    for (size_t i = 0; i < features.rows(); ++i) {
        json featureVector = json::array();
        for (size_t j = 0; j < features.cols(); ++j) {
            featureVector.push_back(features(i, j));
        }
        response["features"].push_back(featureVector);
    }
    
    response["totalFeatures"] = features.rows();
    response["featureDimension"] = features.cols();
    
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