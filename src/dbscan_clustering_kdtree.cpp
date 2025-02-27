#include "dbscan_clustering_kdtree.h"
#include <queue>
#include <iostream>
#include <stdexcept>
#include <stack>
#include <numeric>

namespace logai {

// K-d tree node structure
struct KDNode {
    size_t point_idx;           // Index of the point in the original data
    KDNode* left = nullptr;     // Left child (points with smaller values in splitting dimension)
    KDNode* right = nullptr;    // Right child (points with larger values in splitting dimension)
    
    KDNode(size_t idx) : point_idx(idx) {}
    ~KDNode() {
        delete left;
        delete right;
    }
};

// KDTree implementation
KDTree::KDTree(const std::vector<std::vector<float>>& data) : data_(data), root_(nullptr) {
    if (data_.empty()) {
        throw std::invalid_argument("Cannot build k-d tree from empty data");
    }
    
    dimensions_ = data_[0].size();
    if (dimensions_ == 0) {
        throw std::invalid_argument("Data points must have at least one dimension");
    }
    
    // Create indices for all points
    std::vector<size_t> points(data_.size());
    std::iota(points.begin(), points.end(), 0);
    
    // Build the tree recursively
    root_ = build_tree(points, 0);
}

KDTree::~KDTree() {
    delete root_;
}

KDNode* KDTree::build_tree(const std::vector<size_t>& points, int depth) {
    if (points.empty()) {
        return nullptr;
    }
    
    // Select axis based on depth (cycle through dimensions)
    int axis = depth % dimensions_;
    
    // Sort points based on the selected axis
    std::vector<size_t> sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(), [&](size_t a, size_t b) {
        return data_[a][axis] < data_[b][axis];
    });
    
    // Get the median point
    size_t median_idx = sorted_points.size() / 2;
    
    // Create node and construct subtrees
    KDNode* node = new KDNode(sorted_points[median_idx]);
    
    // Left subtree with points before median
    std::vector<size_t> left_points(sorted_points.begin(), sorted_points.begin() + median_idx);
    node->left = build_tree(left_points, depth + 1);
    
    // Right subtree with points after median
    std::vector<size_t> right_points(sorted_points.begin() + median_idx + 1, sorted_points.end());
    node->right = build_tree(right_points, depth + 1);
    
    return node;
}

float KDTree::squared_distance(const std::vector<float>& p1, const std::vector<float>& p2) const {
    float sum = 0.0f;
    for (size_t i = 0; i < p1.size(); ++i) {
        const float diff = p1[i] - p2[i];
        sum += diff * diff;
    }
    return sum;
}

std::vector<size_t> KDTree::radius_search(const std::vector<float>& query, float radius) const {
    std::vector<size_t> results;
    if (root_ == nullptr) {
        return results;
    }
    
    // Convert radius to squared radius for efficiency
    float squared_radius = radius * radius;
    
    // Start recursive search
    search_radius(root_, query, squared_radius, 0, results);
    
    return results;
}

void KDTree::search_radius(const KDNode* node, const std::vector<float>& query, 
                          float squared_radius, int depth, std::vector<size_t>& results) const {
    if (node == nullptr) {
        return;
    }
    
    // Check if current point is within radius
    const auto& point = data_[node->point_idx];
    float dist = squared_distance(point, query);
    if (dist <= squared_radius) {
        results.push_back(node->point_idx);
    }
    
    // Select axis based on depth
    int axis = depth % dimensions_;
    
    // Compute distance from query point to splitting plane
    float dist_to_plane = query[axis] - point[axis];
    float squared_dist_to_plane = dist_to_plane * dist_to_plane;
    
    // Determine which subtree to search first
    KDNode* near_subtree = (dist_to_plane <= 0) ? node->left : node->right;
    KDNode* far_subtree = (dist_to_plane <= 0) ? node->right : node->left;
    
    // Always search the near subtree
    search_radius(near_subtree, query, squared_radius, depth + 1, results);
    
    // Only search the far subtree if it could contain points within radius
    if (squared_dist_to_plane <= squared_radius) {
        search_radius(far_subtree, query, squared_radius, depth + 1, results);
    }
}

// DBSCAN implementation with KD-tree optimization
DbScanClusteringKDTree::DbScanClusteringKDTree(const DbScanKDTreeParams& params) : params_(params) {
    if (params_.eps <= 0) {
        throw std::invalid_argument("eps must be positive");
    }
    if (params_.min_samples < 1) {
        throw std::invalid_argument("min_samples must be at least 1");
    }
}

void DbScanClusteringKDTree::fit(const std::vector<std::vector<float>>& data) {
    if (data.empty()) {
        throw std::invalid_argument("Input data cannot be empty");
    }
    
    // Store the data and initialize labels
    data_ = data;
    const size_t n_samples = data_.size();
    labels_.assign(n_samples, -1); // -1 indicates an unvisited point
    
    // Build the k-d tree
    kdtree_ = std::make_unique<KDTree>(data_);
    
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

std::vector<int> DbScanClusteringKDTree::get_labels() const {
    return labels_;
}

std::vector<size_t> DbScanClusteringKDTree::region_query(size_t point_idx) const {
    return kdtree_->radius_search(data_[point_idx], params_.eps);
}

void DbScanClusteringKDTree::expand_cluster(size_t point_idx, const std::vector<size_t>& neighbors, int cluster_id) {
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