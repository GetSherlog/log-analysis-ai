#include "one_class_svm.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>
#include <cassert>
#include <Eigen/Dense>

using namespace logai;

// Generate synthetic data with outliers
void generate_synthetic_data(Eigen::MatrixXd& train_data, Eigen::MatrixXd& test_data, 
                             Eigen::VectorXd& test_labels, int n_samples=100, int n_features=2) {
    // Create a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Generate normal data for training (centered at origin)
    std::normal_distribution<double> normal_dist(0.0, 1.0);
    train_data = Eigen::MatrixXd(n_samples, n_features);
    
    for (int i = 0; i < n_samples; ++i) {
        for (int j = 0; j < n_features; ++j) {
            train_data(i, j) = normal_dist(gen);
        }
    }
    
    // Generate test data with both normal points and outliers
    int n_test = n_samples / 2;
    int n_outliers = n_test / 5;  // 20% outliers
    int n_normal = n_test - n_outliers;
    
    test_data = Eigen::MatrixXd(n_test, n_features);
    test_labels = Eigen::VectorXd(n_test);
    
    // Generate normal test points
    for (int i = 0; i < n_normal; ++i) {
        for (int j = 0; j < n_features; ++j) {
            test_data(i, j) = normal_dist(gen);
        }
        test_labels(i) = 1;  // Normal point
    }
    
    // Generate outliers (points far from origin)
    std::uniform_real_distribution<double> outlier_dist(-10.0, 10.0);
    for (int i = n_normal; i < n_test; ++i) {
        for (int j = 0; j < n_features; ++j) {
            test_data(i, j) = outlier_dist(gen);
        }
        test_labels(i) = -1;  // Outlier
    }
}

// Calculate accuracy of predictions compared to true labels
double calculate_accuracy(const Eigen::VectorXd& predictions, const Eigen::VectorXd& true_labels) {
    if (predictions.size() != true_labels.size()) {
        throw std::invalid_argument("Prediction and label vectors must have the same size");
    }
    
    int n_correct = 0;
    for (int i = 0; i < predictions.size(); ++i) {
        if (predictions(i) == true_labels(i)) {
            n_correct++;
        }
    }
    
    return static_cast<double>(n_correct) / predictions.size();
}

int main() {
    std::cout << "Testing One-Class SVM for Anomaly Detection" << std::endl;
    
    // Generate synthetic data
    Eigen::MatrixXd train_data, test_data;
    Eigen::VectorXd test_labels;
    generate_synthetic_data(train_data, test_data, test_labels);
    
    std::cout << "Generated " << train_data.rows() << " training points and " 
              << test_data.rows() << " test points" << std::endl;
    
    // Create and configure One-Class SVM
    OneClassSVMParams params;
    params.kernel = "rbf";
    params.nu = 0.1;
    params.gamma = "auto";
    params.verbose = true;
    
    // Train the model
    OneClassSVMDetector detector(params);
    auto start_time = std::chrono::high_resolution_clock::now();
    
    Eigen::VectorXd train_scores = detector.fit(train_data);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    
    std::cout << "Training completed in " << elapsed.count() << " seconds" << std::endl;
    
    // Test the model
    start_time = std::chrono::high_resolution_clock::now();
    
    Eigen::VectorXd predictions = detector.predict(test_data);
    Eigen::VectorXd test_scores = detector.score_samples(test_data);
    
    end_time = std::chrono::high_resolution_clock::now();
    elapsed = end_time - start_time;
    
    std::cout << "Prediction completed in " << elapsed.count() << " seconds" << std::endl;
    
    // Calculate accuracy
    double accuracy = calculate_accuracy(predictions, test_labels);
    std::cout << "Accuracy on test data: " << accuracy * 100 << "%" << std::endl;
    
    // Print some sample predictions
    std::cout << "\nSample predictions:" << std::endl;
    std::cout << "Index | True Label | Prediction | Score" << std::endl;
    std::cout << "-------------------------------" << std::endl;
    
    int n_samples_to_show = std::min(10, static_cast<int>(test_data.rows()));
    for (int i = 0; i < n_samples_to_show; ++i) {
        std::cout << i << " | " 
                  << test_labels(i) << " | " 
                  << predictions(i) << " | " 
                  << test_scores(i) << std::endl;
    }
    
    // Verify predictions are not all the same
    double sum = predictions.sum();
    assert(sum != predictions.size() && sum != -predictions.size() && 
           "All predictions are the same - model is not discriminating");
    
    std::cout << "\nTest completed successfully!" << std::endl;
    return 0;
} 