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

AnomalyDetectionController::AnomalyDetectionController()
    : apiController_(std::make_shared<ApiController>()),
      featureExtractor_(std::make_unique<FeatureExtractor>()),
      labelEncoder_(std::make_unique<LabelEncoder>()),
      logbertVectorizer_(std::make_unique<LogbertVectorizer>()),
      oneClassSvm_(std::make_unique<OneClassSVM>()),
      dbscan_(std::make_unique<DbscanClustering>()),
      dbscanKdtree_(std::make_unique<DbscanClusteringKDTree>()) {
    
    // Initialize with default parameters
    oneClassSvm_->setKernel("rbf");
    oneClassSvm_->setNu(0.1);
    oneClassSvm_->setGamma(0.1);
    
    dbscan_->setEps(0.5);
    dbscan_->setMinSamples(5);
    
    dbscanKdtree_->setEps(0.5);
    dbscanKdtree_->setMinSamples(5);
}

void AnomalyDetectionController::extractFeatures(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!apiController_->parseJsonBody(req, requestJson)) {
        callback(apiController_->createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("logLines") || !requestJson["logLines"].is_array()) {
        callback(apiController_->createErrorResponse("Request must contain 'logLines' array"));
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
        callback(apiController_->createErrorResponse("No valid log lines provided"));
        return;
    }
    
    // Extract features
    auto features = featureExtractor_->extract(logLines);
    
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
    
    callback(apiController_->createJsonResponse(response));
}

void AnomalyDetectionController::vectorizeLogbert(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!apiController_->parseJsonBody(req, requestJson)) {
        callback(apiController_->createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("logMessages") || !requestJson["logMessages"].is_array()) {
        callback(apiController_->createErrorResponse("Request must contain 'logMessages' array"));
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
        callback(apiController_->createErrorResponse("No valid log messages provided"));
        return;
    }
    
    // Optional configuration
    if (requestJson.contains("maxSequenceLength") && requestJson["maxSequenceLength"].is_number_integer()) {
        logbertVectorizer_->setMaxSequenceLength(requestJson["maxSequenceLength"].get<int>());
    }
    
    // Vectorize messages
    auto vectors = logbertVectorizer_->vectorize(logMessages);
    
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
    
    callback(apiController_->createJsonResponse(response));
}

void AnomalyDetectionController::detectAnomaliesOcSvm(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!apiController_->parseJsonBody(req, requestJson)) {
        callback(apiController_->createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("featureVectors") || !requestJson["featureVectors"].is_array()) {
        callback(apiController_->createErrorResponse("Request must contain 'featureVectors' array"));
        return;
    }
    
    // Parse feature vectors
    const auto& featureVectorsJson = requestJson["featureVectors"];
    size_t numSamples = featureVectorsJson.size();
    
    if (numSamples == 0) {
        callback(apiController_->createErrorResponse("No feature vectors provided"));
        return;
    }
    
    size_t numFeatures = featureVectorsJson[0].size();
    MatrixXd featureMatrix(numSamples, numFeatures);
    
    for (size_t i = 0; i < numSamples; ++i) {
        const auto& vector = featureVectorsJson[i];
        if (!vector.is_array() || vector.size() != numFeatures) {
            callback(apiController_->createErrorResponse("All feature vectors must have the same dimension"));
            return;
        }
        
        for (size_t j = 0; j < numFeatures; ++j) {
            if (!vector[j].is_number()) {
                callback(apiController_->createErrorResponse("Feature values must be numeric"));
                return;
            }
            featureMatrix(i, j) = vector[j].get<double>();
        }
    }
    
    // Configure OneClassSVM parameters
    if (requestJson.contains("kernel") && requestJson["kernel"].is_string()) {
        oneClassSvm_->setKernel(requestJson["kernel"].get<std::string>());
    }
    
    if (requestJson.contains("nu") && requestJson["nu"].is_number()) {
        oneClassSvm_->setNu(requestJson["nu"].get<double>());
    }
    
    if (requestJson.contains("gamma") && requestJson["gamma"].is_number()) {
        oneClassSvm_->setGamma(requestJson["gamma"].get<double>());
    }
    
    // Train and predict
    oneClassSvm_->fit(featureMatrix);
    VectorXd predictions = oneClassSvm_->predict(featureMatrix);
    
    // Prepare response
    json response;
    response["predictions"] = json::array();
    response["anomalyIndices"] = json::array();
    
    for (size_t i = 0; i < predictions.size(); ++i) {
        response["predictions"].push_back(predictions(i));
        if (predictions(i) < 0) {  // Anomaly has negative value in OC-SVM
            response["anomalyIndices"].push_back(i);
        }
    }
    
    response["totalSamples"] = numSamples;
    response["anomalyCount"] = response["anomalyIndices"].size();
    
    callback(apiController_->createJsonResponse(response));
}

void AnomalyDetectionController::clusterDbscan(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    
    // Parse request JSON
    json requestJson;
    if (!apiController_->parseJsonBody(req, requestJson)) {
        callback(apiController_->createErrorResponse("Invalid JSON in request body"));
        return;
    }
    
    // Validate request
    if (!requestJson.contains("featureVectors") || !requestJson["featureVectors"].is_array()) {
        callback(apiController_->createErrorResponse("Request must contain 'featureVectors' array"));
        return;
    }
    
    // Parse feature vectors
    const auto& featureVectorsJson = requestJson["featureVectors"];
    size_t numSamples = featureVectorsJson.size();
    
    if (numSamples == 0) {
        callback(apiController_->createErrorResponse("No feature vectors provided"));
        return;
    }
    
    size_t numFeatures = featureVectorsJson[0].size();
    MatrixXd featureMatrix(numSamples, numFeatures);
    
    for (size_t i = 0; i < numSamples; ++i) {
        const auto& vector = featureVectorsJson[i];
        if (!vector.is_array() || vector.size() != numFeatures) {
            callback(apiController_->createErrorResponse("All feature vectors must have the same dimension"));
            return;
        }
        
        for (size_t j = 0; j < numFeatures; ++j) {
            if (!vector[j].is_number()) {
                callback(apiController_->createErrorResponse("Feature values must be numeric"));
                return;
            }
            featureMatrix(i, j) = vector[j].get<double>();
        }
    }
    
    // Configure DBSCAN parameters
    bool useKdTree = false;
    if (requestJson.contains("useKdTree") && requestJson["useKdTree"].is_boolean()) {
        useKdTree = requestJson["useKdTree"].get<bool>();
    }
    
    if (requestJson.contains("eps") && requestJson["eps"].is_number()) {
        double eps = requestJson["eps"].get<double>();
        if (useKdTree) {
            dbscanKdtree_->setEps(eps);
        } else {
            dbscan_->setEps(eps);
        }
    }
    
    if (requestJson.contains("minSamples") && requestJson["minSamples"].is_number_integer()) {
        int minSamples = requestJson["minSamples"].get<int>();
        if (useKdTree) {
            dbscanKdtree_->setMinSamples(minSamples);
        } else {
            dbscan_->setMinSamples(minSamples);
        }
    }
    
    // Perform clustering
    VectorXd labels;
    if (useKdTree) {
        labels = dbscanKdtree_->fit_predict(featureMatrix);
    } else {
        labels = dbscan_->fit_predict(featureMatrix);
    }
    
    // Count clusters
    int numClusters = 0;
    for (size_t i = 0; i < labels.size(); ++i) {
        if (labels(i) > numClusters) {
            numClusters = labels(i);
        }
    }
    numClusters++; // Add one since cluster labels start at 0
    
    // Count outliers (label -1)
    int outlierCount = 0;
    for (size_t i = 0; i < labels.size(); ++i) {
        if (labels(i) == -1) {
            outlierCount++;
        }
    }
    
    // Prepare response
    json response;
    response["labels"] = json::array();
    
    for (size_t i = 0; i < labels.size(); ++i) {
        response["labels"].push_back(labels(i));
    }
    
    response["totalSamples"] = numSamples;
    response["numClusters"] = numClusters;
    response["outlierCount"] = outlierCount;
    response["useKdTree"] = useKdTree;
    
    callback(apiController_->createJsonResponse(response));
}

} // namespace web
} // namespace logai 