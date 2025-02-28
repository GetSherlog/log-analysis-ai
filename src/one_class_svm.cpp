#include "one_class_svm.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <limits>

namespace logai {

namespace {
// Simple SMO algorithm for One-Class SVM training
// Based on the algorithm described in "A Tutorial on Support Vector Machines for Pattern Recognition" by Christopher Burges
class SMOSolver {
public:
    SMOSolver(const Eigen::MatrixXd& features, double nu, 
              std::function<double(const Eigen::VectorXd&, const Eigen::VectorXd&)> kernel_fn,
              double tolerance, bool verbose)
        : X_(features), 
          n_samples_(features.rows()), 
          nu_(nu), 
          kernel_fn_(kernel_fn),
          tol_(tolerance),
          verbose_(verbose) {
        
        // Initialize alpha and calculate kernel matrix
        alpha_ = Eigen::VectorXd::Zero(n_samples_);
        K_ = Eigen::MatrixXd::Zero(n_samples_, n_samples_);
        
        for (int i = 0; i < n_samples_; ++i) {
            for (int j = i; j < n_samples_; ++j) {
                double k_val = kernel_fn_(X_.row(i), X_.row(j));
                K_(i, j) = k_val;
                K_(j, i) = k_val; // Kernel matrix is symmetric
            }
        }
    }
    
    // Main optimization routine
    void optimize(int max_iterations = 1000) {
        // Initialize variables
        Eigen::VectorXd f = Eigen::VectorXd::Constant(n_samples_, -1.0);
        
        // Set initial values for two alphas
        // Constraint: sum(alpha_i) = nu * n_samples
        double initial_alpha_value = nu_ * n_samples_ / 2.0;
        alpha_(0) = initial_alpha_value;
        alpha_(1) = initial_alpha_value;
        
        // Update f
        for (int i = 0; i < n_samples_; ++i) {
            for (int j = 0; j < n_samples_; ++j) {
                f(i) += alpha_(j) * K_(j, i);
            }
        }
        
        // Main loop
        int iter = 0;
        bool converged = false;
        
        while (iter < max_iterations && !converged) {
            int num_changed_alphas = 0;
            
            // Loop over all examples
            for (int i = 0; i < n_samples_; ++i) {
                double E_i = f(i);
                
                // Check if example violates KKT conditions
                if ((alpha_(i) < tol_ && E_i < -tol_) ||
                    (alpha_(i) > tol_ && E_i > tol_)) {
                    
                    // Select second example j randomly
                    int j;
                    do {
                        j = rand() % n_samples_;
                    } while (j == i);
                    
                    double E_j = f(j);
                    
                    // Save old alphas
                    double alpha_i_old = alpha_(i);
                    double alpha_j_old = alpha_(j);
                    
                    // Compute L and H
                    double L = std::max(0.0, alpha_(i) + alpha_(j) - nu_ * n_samples_);
                    double H = std::min(nu_ * n_samples_, alpha_(i) + alpha_(j));
                    
                    if (L >= H) {
                        continue;
                    }
                    
                    // Compute eta
                    double eta = 2 * K_(i, j) - K_(i, i) - K_(j, j);
                    
                    if (eta >= 0) {
                        continue;
                    }
                    
                    // Compute new alpha_j
                    alpha_(j) = alpha_j_old - (E_i - E_j) / eta;
                    
                    // Clip alpha_j
                    alpha_(j) = std::min(H, std::max(L, alpha_(j)));
                    
                    // Check if alpha_j changed significantly
                    if (std::abs(alpha_(j) - alpha_j_old) < tol_) {
                        continue;
                    }
                    
                    // Compute new alpha_i
                    alpha_(i) = alpha_i_old + (alpha_j_old - alpha_(j));
                    
                    // Update f for all examples
                    for (int k = 0; k < n_samples_; ++k) {
                        f(k) += (alpha_(i) - alpha_i_old) * K_(i, k) +
                                (alpha_(j) - alpha_j_old) * K_(j, k);
                    }
                    
                    num_changed_alphas++;
                }
            }
            
            if (num_changed_alphas == 0) {
                converged = true;
            } else if (verbose_ && iter % 10 == 0) {
                std::cout << "Iteration " << iter << ", changed alphas: " << num_changed_alphas << std::endl;
            }
            
            iter++;
        }
        
        if (verbose_) {
            std::cout << "SVM training completed after " << iter << " iterations" << std::endl;
        }
        
        // Compute rho (intercept)
        compute_rho();
        
        // Identify support vectors
        identify_support_vectors();
    }
    
    // Compute the bias term rho
    void compute_rho() {
        std::vector<double> rho_values;
        
        for (int i = 0; i < n_samples_; ++i) {
            if (alpha_(i) > tol_ && alpha_(i) < nu_ - tol_) {
                double sum = 0.0;
                for (int j = 0; j < n_samples_; ++j) {
                    sum += alpha_(j) * K_(j, i);
                }
                rho_values.push_back(sum);
            }
        }
        
        if (rho_values.empty()) {
            // If no free support vectors, use the average of f values for all support vectors
            rho_values.clear();
            for (int i = 0; i < n_samples_; ++i) {
                if (alpha_(i) > tol_) {
                    double sum = 0.0;
                    for (int j = 0; j < n_samples_; ++j) {
                        sum += alpha_(j) * K_(j, i);
                    }
                    rho_values.push_back(sum);
                }
            }
        }
        
        if (rho_values.empty()) {
            rho_ = 0.0;
        } else {
            rho_ = std::accumulate(rho_values.begin(), rho_values.end(), 0.0) / rho_values.size();
        }
    }
    
    // Identify support vectors and prepare final model
    void identify_support_vectors() {
        std::vector<int> sv_indices;
        std::vector<double> sv_coeffs;
        
        for (int i = 0; i < n_samples_; ++i) {
            if (alpha_(i) > tol_) {
                sv_indices.push_back(i);
                sv_coeffs.push_back(alpha_(i));
            }
        }
        
        n_support_vectors_ = sv_indices.size();
        
        if (n_support_vectors_ == 0) {
            throw std::runtime_error("No support vectors found. Try adjusting the nu parameter.");
        }
        
        // Extract support vectors and their coefficients
        support_vectors_ = Eigen::MatrixXd(n_support_vectors_, X_.cols());
        dual_coefs_ = Eigen::VectorXd(n_support_vectors_);
        
        for (int i = 0; i < n_support_vectors_; ++i) {
            support_vectors_.row(i) = X_.row(sv_indices[i]);
            dual_coefs_(i) = sv_coeffs[i];
        }
    }
    
    Eigen::MatrixXd get_support_vectors() const {
        return support_vectors_;
    }
    
    Eigen::VectorXd get_dual_coefs() const {
        return dual_coefs_;
    }
    
    double get_rho() const {
        return rho_;
    }
    
    int get_n_support_vectors() const {
        return n_support_vectors_;
    }
    
private:
    const Eigen::MatrixXd& X_;
    int n_samples_;
    double nu_;
    std::function<double(const Eigen::VectorXd&, const Eigen::VectorXd&)> kernel_fn_;
    double tol_;
    bool verbose_;
    
    Eigen::VectorXd alpha_;
    Eigen::MatrixXd K_;
    double rho_ = 0.0;
    
    Eigen::MatrixXd support_vectors_;
    Eigen::VectorXd dual_coefs_;
    int n_support_vectors_ = 0;
};
} // anonymous namespace

OneClassSVMDetector::OneClassSVMDetector(const OneClassSVMParams& params)
    : params_(params), rho_(0.0), gamma_value_(0.0) {
    
    // Validate parameters
    if (params_.kernel != "linear" && params_.kernel != "rbf" && 
        params_.kernel != "poly" && params_.kernel != "sigmoid") {
        throw std::invalid_argument("Unsupported kernel type: " + params_.kernel);
    }
    
    if (params_.nu <= 0 || params_.nu > 1) {
        throw std::invalid_argument("nu must be in (0, 1]");
    }
    
    if (params_.degree < 1) {
        throw std::invalid_argument("degree must be >= 1");
    }
}

Eigen::VectorXd OneClassSVMDetector::fit(const Eigen::MatrixXd& log_features) {
    if (log_features.rows() == 0 || log_features.cols() == 0) {
        throw std::invalid_argument("Input data cannot be empty");
    }
    
    // Set gamma value based on the input parameters
    if (params_.gamma == "auto") {
        gamma_value_ = 1.0 / log_features.cols();
    } else if (params_.gamma == "scale") {
        gamma_value_ = 1.0 / (log_features.cols() * log_features.variance());
    } else {
        try {
            gamma_value_ = std::stod(params_.gamma);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid gamma parameter: " + params_.gamma);
        }
    }
    
    // Create a kernel function based on the selected kernel type
    auto kernel_fn = [this](const Eigen::VectorXd& x, const Eigen::VectorXd& y) {
        return this->kernel_function(x, y);
    };
    
    // Train the SVM model using SMO algorithm
    SMOSolver smo(log_features, params_.nu, kernel_fn, params_.tol, params_.verbose);
    smo.optimize();
    
    // Get model parameters from the solver
    support_vectors_ = smo.get_support_vectors();
    dual_coefs_ = smo.get_dual_coefs();
    rho_ = smo.get_rho();
    
    if (params_.verbose) {
        std::cout << "One-Class SVM trained with " << smo.get_n_support_vectors() 
                  << " support vectors" << std::endl;
    }
    
    // Calculate training scores
    return score_samples(log_features);
}

Eigen::VectorXd OneClassSVMDetector::predict(const Eigen::MatrixXd& log_features) const {
    if (support_vectors_.rows() == 0) {
        throw std::runtime_error("Model not trained. Call fit() first.");
    }
    
    Eigen::VectorXd decision_values = score_samples(log_features);
    Eigen::VectorXd predictions(decision_values.size());
    
    // Convert decision values to binary predictions (-1 for outliers, +1 for inliers)
    for (int i = 0; i < decision_values.size(); ++i) {
        predictions(i) = (decision_values(i) >= 0) ? 1.0 : -1.0;
    }
    
    return predictions;
}

Eigen::VectorXd OneClassSVMDetector::score_samples(const Eigen::MatrixXd& log_features) const {
    if (support_vectors_.rows() == 0) {
        throw std::runtime_error("Model not trained. Call fit() first.");
    }
    
    const int n_samples = log_features.rows();
    Eigen::VectorXd scores(n_samples);
    
    // Calculate decision function values for each sample
    for (int i = 0; i < n_samples; ++i) {
        double decision_value = 0.0;
        
        for (int j = 0; j < support_vectors_.rows(); ++j) {
            decision_value += dual_coefs_(j) * kernel_function(log_features.row(i), support_vectors_.row(j));
        }
        
        scores(i) = decision_value - rho_;
    }
    
    return scores;
}

double OneClassSVMDetector::kernel_function(const Eigen::VectorXd& x, const Eigen::VectorXd& y) const {
    if (params_.kernel == "linear") {
        return linear_kernel(x, y);
    } else if (params_.kernel == "rbf") {
        return rbf_kernel(x, y);
    } else if (params_.kernel == "poly") {
        return poly_kernel(x, y);
    } else if (params_.kernel == "sigmoid") {
        return sigmoid_kernel(x, y);
    }
    
    // Default to linear kernel
    return linear_kernel(x, y);
}

double OneClassSVMDetector::rbf_kernel(const Eigen::VectorXd& x, const Eigen::VectorXd& y) const {
    double norm_squared = (x - y).squaredNorm();
    return std::exp(-gamma_value_ * norm_squared);
}

double OneClassSVMDetector::linear_kernel(const Eigen::VectorXd& x, const Eigen::VectorXd& y) const {
    return x.dot(y);
}

double OneClassSVMDetector::poly_kernel(const Eigen::VectorXd& x, const Eigen::VectorXd& y) const {
    return std::pow(gamma_value_ * x.dot(y) + params_.coef0, params_.degree);
}

double OneClassSVMDetector::sigmoid_kernel(const Eigen::VectorXd& x, const Eigen::VectorXd& y) const {
    return std::tanh(gamma_value_ * x.dot(y) + params_.coef0);
}

} // namespace logai 