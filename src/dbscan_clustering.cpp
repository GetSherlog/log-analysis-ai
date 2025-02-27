#include "dbscan_clustering.h"
#include <queue>
#include <iostream>
#include <stdexcept>

namespace logai {

DbScanClustering::DbScanClustering(const DbScanParams& params) : params_(params) {
    // Validate parameters
    if (params_.eps <= 0) {
        throw std::invalid_argument("eps must be positive");
    }
    if (params_.min_samples < 1) {
        throw std::invalid_argument("min_samples must be at least 1");
    }
    if (params_.metric != "euclidean") {
        throw std::invalid_argument("Currently only 'euclidean' metric is supported");
    }
}

void DbScanClustering::fit(const std::vector<std::vector<float>>& data) {
    if (data.empty()) {
        throw std::invalid_argument("Input data cannot be empty");
    }
    
    // Store the data and initialize labels
    data_ = data;
    const size_t n_samples = data_.size();
    labels_.assign(n_samples, -1); // -1 indicates an unvisited point
    
    // Cluster ID counter (starting from 0)
    int cluster_id = 0;
    
    // Iterate through each point
    for (size_t point_idx = 0; point_idx < n_samples; ++point_idx) {
        // Skip points that have already been visited
        if (labels_[point_idx] != -1) {
            continue;
        }
        
        // Find neighbors of the current point
        std::vector<size_t> neighbors = region_query(point_idx);
        
        // If the point has fewer neighbors than min_samples, mark it as noise for now
        if (neighbors.size() < static_cast<size_t>(params_.min_samples)) {
            labels_[point_idx] = -1; // Mark as noise
            continue;
        }
        
        // Start a new cluster
        labels_[point_idx] = cluster_id;
        expand_cluster(point_idx, neighbors, cluster_id);
        
        // Increment cluster ID
        ++cluster_id;
    }
}

std::vector<int> DbScanClustering::get_labels() const {
    return labels_;
}

float DbScanClustering::compute_distance(const std::vector<float>& p1, const std::vector<float>& p2) const {
    if (p1.size() != p2.size()) {
        throw std::invalid_argument("Points must have the same dimension");
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < p1.size(); ++i) {
        const float diff = p1[i] - p2[i];
        sum += diff * diff;
    }
    
    return std::sqrt(sum);
}

std::vector<size_t> DbScanClustering::region_query(size_t point_idx) const {
    const auto& point = data_[point_idx];
    std::vector<size_t> neighbors;
    
    for (size_t i = 0; i < data_.size(); ++i) {
        if (compute_distance(point, data_[i]) <= params_.eps) {
            neighbors.push_back(i);
        }
    }
    
    return neighbors;
}

void DbScanClustering::expand_cluster(size_t point_idx, const std::vector<size_t>& neighbors, int cluster_id) {
    std::queue<size_t> seeds(std::deque<size_t>(neighbors.begin(), neighbors.end()));
    
    while (!seeds.empty()) {
        const size_t current_point = seeds.front();
        seeds.pop();
        
        // If this point is noise, add it to the current cluster
        if (labels_[current_point] == -1) {
            labels_[current_point] = cluster_id;
            
            // Find neighbors of the current point
            std::vector<size_t> current_neighbors = region_query(current_point);
            
            // If this point is a core point, add its neighbors to the seeds
            if (current_neighbors.size() >= static_cast<size_t>(params_.min_samples)) {
                for (const auto& neighbor : current_neighbors) {
                    // If this neighbor is unclassified or noise, add it to the seeds
                    if (labels_[neighbor] == -1) {
                        seeds.push(neighbor);
                    }
                }
            }
        }
        
        // If this point already belongs to another cluster, do nothing
        // (we keep the label unchanged)
    }
}

} // namespace logai 