#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <chrono>
#include "dbscan_clustering.h"

// Simple helper function to load test data
std::vector<std::vector<float>> load_test_data(const std::string& file_path) {
    std::vector<std::vector<float>> data;
    std::ifstream file(file_path);
    std::string line;
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        return data;
    }
    
    while (std::getline(file, line)) {
        std::vector<float> point;
        std::stringstream ss(line);
        std::string value;
        
        while (std::getline(ss, value, ',')) {
            point.push_back(std::stof(value));
        }
        
        if (!point.empty()) {
            data.push_back(point);
        }
    }
    
    file.close();
    return data;
}

// Save clustering results to a CSV file for visualization
void save_results(const std::string& output_file, 
                  const std::vector<std::vector<float>>& data, 
                  const std::vector<int>& labels) {
    std::ofstream file(output_file);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open output file " << output_file << std::endl;
        return;
    }
    
    // Write header
    file << "x,y,cluster" << std::endl;
    
    // Write data points with their cluster labels
    for (size_t i = 0; i < data.size(); ++i) {
        for (size_t j = 0; j < data[i].size(); ++j) {
            file << data[i][j];
            if (j < data[i].size() - 1) {
                file << ",";
            }
        }
        file << "," << labels[i] << std::endl;
    }
    
    file.close();
    std::cout << "Results saved to " << output_file << std::endl;
}

// Function to demonstrate basic DBSCAN usage
void test_synthetic_data() {
    // Create a simple 2D dataset with 3 clusters
    std::vector<std::vector<float>> data = {
        // Cluster 1
        {1.0f, 1.0f}, {1.2f, 0.8f}, {0.9f, 1.1f}, {1.1f, 0.9f},
        // Cluster 2
        {4.0f, 4.0f}, {4.2f, 3.8f}, {3.9f, 4.1f}, {4.1f, 3.9f},
        // Cluster 3
        {1.0f, 4.0f}, {1.2f, 3.8f}, {0.9f, 4.1f}, {1.1f, 3.9f},
        // Noise points
        {2.5f, 2.5f}, {7.0f, 7.0f}
    };
    
    // Set DBSCAN parameters
    logai::DbScanParams params(0.5f, 3, "euclidean");
    
    // Create DBSCAN clusterer
    logai::DbScanClustering dbscan(params);
    
    // Time the clustering operation
    auto start = std::chrono::high_resolution_clock::now();
    
    // Fit the model
    dbscan.fit(data);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Get the cluster labels
    std::vector<int> labels = dbscan.get_labels();
    
    // Print results
    std::cout << "DBSCAN Clustering Results (Synthetic Data)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Parameters: eps = " << params.eps << ", min_samples = " << params.min_samples << std::endl;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
    
    // Count clusters and noise points
    std::map<int, int> cluster_counts;
    for (int label : labels) {
        cluster_counts[label]++;
    }
    
    std::cout << "Number of clusters: " << cluster_counts.size() - (cluster_counts.find(-1) != cluster_counts.end() ? 1 : 0) << std::endl;
    std::cout << "Number of noise points: " << (cluster_counts.find(-1) != cluster_counts.end() ? cluster_counts[-1] : 0) << std::endl;
    
    // Print points and their assigned clusters
    std::cout << "\nPoint\tCluster" << std::endl;
    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << "(" << data[i][0] << ", " << data[i][1] << ")\t" 
                 << (labels[i] == -1 ? "NOISE" : std::to_string(labels[i])) << std::endl;
    }
    
    // Save results to file
    save_results("dbscan_results_synthetic.csv", data, labels);
}

int main(int argc, char* argv[]) {
    // Run synthetic data test
    test_synthetic_data();
    
    // If a file path is provided, run DBSCAN on that data
    if (argc > 1) {
        std::string file_path = argv[1];
        float eps = (argc > 2) ? std::stof(argv[2]) : 0.5f;
        int min_samples = (argc > 3) ? std::stoi(argv[3]) : 5;
        
        std::cout << "\nRunning DBSCAN on file: " << file_path << std::endl;
        std::cout << "Parameters: eps = " << eps << ", min_samples = " << min_samples << std::endl;
        
        // Load data
        auto data = load_test_data(file_path);
        if (data.empty()) {
            return 1;
        }
        
        // Create DBSCAN clusterer
        logai::DbScanParams params(eps, min_samples);
        logai::DbScanClustering dbscan(params);
        
        // Time the clustering operation
        auto start = std::chrono::high_resolution_clock::now();
        
        // Fit the model
        dbscan.fit(data);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Get the cluster labels
        std::vector<int> labels = dbscan.get_labels();
        
        // Count clusters and noise points
        std::map<int, int> cluster_counts;
        for (int label : labels) {
            cluster_counts[label]++;
        }
        
        std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
        std::cout << "Number of clusters: " << cluster_counts.size() - (cluster_counts.find(-1) != cluster_counts.end() ? 1 : 0) << std::endl;
        std::cout << "Number of noise points: " << (cluster_counts.find(-1) != cluster_counts.end() ? cluster_counts[-1] : 0) << std::endl;
        
        // Save results to file
        save_results("dbscan_results_file.csv", data, labels);
    }
    
    return 0;
} 