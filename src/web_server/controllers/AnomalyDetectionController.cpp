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

void AnomalyDetectionController::detectAnomaliesOcSvm(const HttpRequestPtr& req,
                                                     std::function<void(const HttpResponsePtr&)>&& callback) {
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

    if (requestJson.contains("kernel")) {
        (*oneClassSvm_).setKernel(requestJson["kernel"].get<std::string>());
    }

    if (requestJson.contains("nu")) {
        (*oneClassSvm_).setNu(requestJson["nu"].get<double>());
    }

    if (requestJson.contains("gamma")) {
        (*oneClassSvm_).setGamma(requestJson["gamma"].get<double>());
    }

    (*oneClassSvm_).fit(featureMatrix);
    Eigen::VectorXd predictions = (*oneClassSvm_).predict(featureMatrix);

    json response;
    response["predictions"] = std::vector<int>();
    for (size_t i = 0; i < predictions.size(); ++i) {
        response["predictions"].push_back(predictions(i));
    }

    callback(createJsonResponse(response));
}

void AnomalyDetectionController::clusterDbscan(const HttpRequestPtr& req,
                                             std::function<void(const HttpResponsePtr&)>&& callback) {
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

    bool useKdTree = requestJson.value("useKdTree", false);
    Eigen::VectorXd labels;

    if (requestJson.contains("eps")) {
        double eps = requestJson["eps"].get<double>();
        if (useKdTree) {
            (*dbscanKdtree_).setEps(eps);
        } else {
            (*dbscan_).setEps(eps);
        }
    }

    if (requestJson.contains("minSamples")) {
        int minSamples = requestJson["minSamples"].get<int>();
        if (useKdTree) {
            (*dbscanKdtree_).setMinSamples(minSamples);
        } else {
            (*dbscan_).setMinSamples(minSamples);
        }
    }

    if (useKdTree) {
        labels = (*dbscanKdtree_).fit_predict(featureMatrix);
    } else {
        labels = (*dbscan_).fit_predict(featureMatrix);
    }

    json response;
    response["labels"] = std::vector<int>();
    for (size_t i = 0; i < labels.size(); ++i) {
        response["labels"].push_back(labels(i));
    }

    // Count number of clusters
    std::set<int> uniqueLabels;
    for (size_t i = 0; i < labels.size(); ++i) {
        uniqueLabels.insert(labels(i));
    }
    response["numClusters"] = uniqueLabels.size();

    // Count points per cluster
    std::map<int, int> clusterCounts;
    for (size_t i = 0; i < labels.size(); ++i) {
        clusterCounts[labels(i)]++;
    }
    response["clusterCounts"] = clusterCounts;

    callback(createJsonResponse(response));
}

} // namespace web
} // namespace logai 