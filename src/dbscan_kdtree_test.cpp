#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <chrono>
#include <random>
#include "dbscan_clustering_kdtree.h"
#include "dbscan_clustering.h" // For comparison

// Helper function to load test data from a file
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

// Generate synthetic dataset with random clusters
std::vector<std::vector<float>> generate_dataset(int num_clusters, int points_per_cluster, int dimensions, float cluster_radius) {
    std::vector<std::vector<float>> data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> cluster_center_dist(-10.0f, 10.0f);
    std::normal_distribution<float> point_dist(0.0f, cluster_radius);
    
    // Generate cluster centers
    std::vector<std::vector<float>> cluster_centers;
    for (int i = 0; i < num_clusters; ++i) {
        std::vector<float> center;
        for (int d = 0; d < dimensions; ++d) {
            center.push_back(cluster_center_dist(gen));
        }
        cluster_centers.push_back(center);
    }
    
    // Generate points around each cluster center
    for (int i = 0; i < num_clusters; ++i) {
        for (int j = 0; j < points_per_cluster; ++j) {
            std::vector<float> point = cluster_centers[i];
            for (int d = 0; d < dimensions; ++d) {
                point[d] += point_dist(gen);
            }
            data.push_back(point);
        }
    }
    
    // Add some noise points
    int noise_points = num_clusters * points_per_cluster / 10; // 10% noise
    for (int i = 0; i < noise_points; ++i) {
        std::vector<float> point;
        for (int d = 0; d < dimensions; ++d) {
            point.push_back(cluster_center_dist(gen) * 2.0f);
        }
        data.push_back(point);
    }
    
    return data;
}

// Comparison test between regular DBSCAN and KD-tree optimized DBSCAN
void compare_dbscan_implementations() {
    std::cout << "Comparing DBSCAN implementations..." << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    
    // Generate synthetic dataset
    const int num_clusters = 5;
    const int points_per_cluster = 50;
    const int dimensions = 2;
    const float cluster_radius = 0.3f;
    const float eps = 0.5f;
    const int min_samples = 5;
    
    std::vector<std::vector<float>> data = generate_dataset(num_clusters, points_per_cluster, dimensions, cluster_radius);
    
    std::cout << "Generated dataset with " << data.size() << " points in " << dimensions << " dimensions" << std::endl;
    std::cout << "Parameters: eps = " << eps << ", min_samples = " << min_samples << std::endl;
    
    // Test regular DBSCAN
    logai::DbScanParams params_regular(eps, min_samples);
    logai::DbScanClustering dbscan_regular(params_regular);
    
    auto start_regular = std::chrono::high_resolution_clock::now();
    dbscan_regular.fit(data);
    auto end_regular = std::chrono::high_resolution_clock::now();
    auto duration_regular = std::chrono::duration_cast<std::chrono::milliseconds>(end_regular - start_regular);
    
    std::vector<int> labels_regular = dbscan_regular.get_labels();
    
    // Count clusters in regular DBSCAN
    std::map<int, int> cluster_counts_regular;
    for (int label : labels_regular) {
        cluster_counts_regular[label]++;
    }
    
    // Test KD-tree optimized DBSCAN
    logai::DbScanKDTreeParams params_kdtree(eps, min_samples);
    logai::DbScanClusteringKDTree dbscan_kdtree(params_kdtree);
    
    auto start_kdtree = std::chrono::high_resolution_clock::now();
    dbscan_kdtree.fit(data);
    auto end_kdtree = std::chrono::high_resolution_clock::now();
    auto duration_kdtree = std::chrono::duration_cast<std::chrono::milliseconds>(end_kdtree - start_kdtree);
    
    std::vector<int> labels_kdtree = dbscan_kdtree.get_labels();
    
    // Count clusters in KD-tree DBSCAN
    std::map<int, int> cluster_counts_kdtree;
    for (int label : labels_kdtree) {
        cluster_counts_kdtree[label]++;
    }
    
    // Print results
    std::cout << "\nRegular DBSCAN results:" << std::endl;
    std::cout << "Execution time: " << duration_regular.count() << " ms" << std::endl;
    std::cout << "Number of clusters: " << cluster_counts_regular.size() - (cluster_counts_regular.find(-1) != cluster_counts_regular.end() ? 1 : 0) << std::endl;
    std::cout << "Number of noise points: " << (cluster_counts_regular.find(-1) != cluster_counts_regular.end() ? cluster_counts_regular[-1] : 0) << std::endl;
    
    std::cout << "\nKD-tree optimized DBSCAN results:" << std::endl;
    std::cout << "Execution time: " << duration_kdtree.count() << " ms" << std::endl;
    std::cout << "Number of clusters: " << cluster_counts_kdtree.size() - (cluster_counts_kdtree.find(-1) != cluster_counts_kdtree.end() ? 1 : 0) << std::endl;
    std::cout << "Number of noise points: " << (cluster_counts_kdtree.find(-1) != cluster_counts_kdtree.end() ? cluster_counts_kdtree[-1] : 0) << std::endl;
    
    std::cout << "\nSpeed improvement: " << (float)duration_regular.count() / duration_kdtree.count() << "x" << std::endl;
    
    // Save results to file
    save_results("dbscan_regular_results.csv", data, labels_regular);
    save_results("dbscan_kdtree_results.csv", data, labels_kdtree);
}

// Test different dataset sizes to demonstrate the efficiency improvement
void test_scalability() {
    std::cout << "\nTesting scalability of DBSCAN implementations..." << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
    
    // Parameters
    const int dimensions = 2;
    const float cluster_radius = 0.3f;
    const float eps = 0.5f;
    const int min_samples = 5;
    
    // Different dataset sizes to test
    std::vector<int> dataset_sizes = {100, 500, 1000, 2000, 5000};
    
    std::cout << "Parameters: eps = " << eps << ", min_samples = " << min_samples << std::endl;
    std::cout << "\nDataset Size\tRegular DBSCAN (ms)\tKD-tree DBSCAN (ms)\tSpeed Improvement" << std::endl;
    std::cout << "------------\t------------------\t------------------\t-----------------" << std::endl;
    
    for (int size : dataset_sizes) {
        // Generate dataset
        int num_clusters = std::max(3, size / 100);
        int points_per_cluster = size / num_clusters;
        
        std::vector<std::vector<float>> data = generate_dataset(num_clusters, points_per_cluster, dimensions, cluster_radius);
        
        // Test regular DBSCAN
        logai::DbScanParams params_regular(eps, min_samples);
        logai::DbScanClustering dbscan_regular(params_regular);
        
        auto start_regular = std::chrono::high_resolution_clock::now();
        dbscan_regular.fit(data);
        auto end_regular = std::chrono::high_resolution_clock::now();
        auto duration_regular = std::chrono::duration_cast<std::chrono::milliseconds>(end_regular - start_regular);
        
        // Test KD-tree optimized DBSCAN
        logai::DbScanKDTreeParams params_kdtree(eps, min_samples);
        logai::DbScanClusteringKDTree dbscan_kdtree(params_kdtree);
        
        auto start_kdtree = std::chrono::high_resolution_clock::now();
        dbscan_kdtree.fit(data);
        auto end_kdtree = std::chrono::high_resolution_clock::now();
        auto duration_kdtree = std::chrono::duration_cast<std::chrono::milliseconds>(end_kdtree - start_kdtree);
        
        // Calculate speed improvement
        float speedup = (float)duration_regular.count() / std::max(1LL, duration_kdtree.count());
        
        // Print results
        std::cout << size << "\t\t" << duration_regular.count() << "\t\t\t" << duration_kdtree.count() 
                  << "\t\t\t" << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Run comparison test
    compare_dbscan_implementations();
    
    // Test scalability
    test_scalability();
    
    // If a file path is provided, run both implementations on the data
    if (argc > 1) {
        std::string file_path = argv[1];
        float eps = (argc > 2) ? std::stof(argv[2]) : 0.5f;
        int min_samples = (argc > 3) ? std::stoi(argv[3]) : 5;
        
        std::cout << "\nRunning both DBSCAN implementations on file: " << file_path << std::endl;
        std::cout << "Parameters: eps = " << eps << ", min_samples = " << min_samples << std::endl;
        
        // Load data
        auto data = load_test_data(file_path);
        if (data.empty()) {
            return 1;
        }
        
        // Test regular DBSCAN
        logai::DbScanParams params_regular(eps, min_samples);
        logai::DbScanClustering dbscan_regular(params_regular);
        
        auto start_regular = std::chrono::high_resolution_clock::now();
        dbscan_regular.fit(data);
        auto end_regular = std::chrono::high_resolution_clock::now();
        auto duration_regular = std::chrono::duration_cast<std::chrono::milliseconds>(end_regular - start_regular);
        
        std::vector<int> labels_regular = dbscan_regular.get_labels();
        
        // Test KD-tree optimized DBSCAN
        logai::DbScanKDTreeParams params_kdtree(eps, min_samples);
        logai::DbScanClusteringKDTree dbscan_kdtree(params_kdtree);
        
        auto start_kdtree = std::chrono::high_resolution_clock::now();
        dbscan_kdtree.fit(data);
        auto end_kdtree = std::chrono::high_resolution_clock::now();
        auto duration_kdtree = std::chrono::duration_cast<std::chrono::milliseconds>(end_kdtree - start_kdtree);
        
        std::vector<int> labels_kdtree = dbscan_kdtree.get_labels();
        
        // Print results
        std::cout << "\nRegular DBSCAN:" << std::endl;
        std::cout << "Execution time: " << duration_regular.count() << " ms" << std::endl;
        
        std::cout << "\nKD-tree optimized DBSCAN:" << std::endl;
        std::cout << "Execution time: " << duration_kdtree.count() << " ms" << std::endl;
        
        std::cout << "\nSpeed improvement: " << (float)duration_regular.count() / duration_kdtree.count() << "x" << std::endl;
        
        // Save results to file
        save_results("dbscan_regular_file_results.csv", data, labels_regular);
        save_results("dbscan_kdtree_file_results.csv", data, labels_kdtree);
    }
    
    return 0;
} 