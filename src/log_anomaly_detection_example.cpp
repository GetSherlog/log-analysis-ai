/**
 * @file log_anomaly_detection_example.cpp
 * @brief Log Anomaly Detection Example using LogAI C++ Implementation
 * 
 * This example demonstrates how to use LogAI C++ implementations for log anomaly detection,
 * following similar steps as shown in the Python tutorial.
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <random>
#include <fstream>
#include <iomanip>
#include <filesystem>

// Arrow includes
#include <arrow/api.h>

// LogAI includes
#include "file_data_loader.h"
#include "data_loader_config.h"
#include "preprocessor.h"
#include "drain_parser.h"
#include "feature_extractor.h"
#include "label_encoder.h"
#include "logbert_vectorizer.h"
#include "one_class_svm.h"
#include "dbscan_clustering.h"
#include "dbscan_clustering_kdtree.h"

// Eigen for matrix operations
#include <Eigen/Dense>

namespace fs = std::filesystem;

// Forward declarations
std::shared_ptr<arrow::Table> log_records_to_arrow_table(
    const std::vector<logai::LogRecordObject>& records);

// Helper function to split a dataset into training and test sets
std::pair<Eigen::MatrixXd, Eigen::MatrixXd> train_test_split(
    const Eigen::MatrixXd& data, double train_size = 0.7) {
    
    int total_rows = data.rows();
    int train_rows = static_cast<int>(total_rows * train_size);
    
    Eigen::MatrixXd train = data.topRows(train_rows);
    Eigen::MatrixXd test = data.bottomRows(total_rows - train_rows);
    
    return {train, test};
}

int main(int argc, char* argv[]) {
    std::cout << "Log Anomaly Detection using LogAI C++" << std::endl;
    std::cout << "============================================" << std::endl;
    
    // Set up paths
    std::string dataset_path;
    if (argc > 1) {
        dataset_path = argv[1];
    } else {
        // Default path to HealthApp dataset
        dataset_path = "../test_data/HealthApp_2000.log";
    }
    
    if (!fs::exists(dataset_path)) {
        std::cerr << "Error: Dataset file not found: " << dataset_path << std::endl;
        std::cerr << "Please provide a valid path to the log file." << std::endl;
        return 1;
    }
    
    std::cout << "Using dataset: " << dataset_path << std::endl;
    
    //=============================================================================
    // Step 1: Load Data
    //=============================================================================
    std::cout << "\n## Step 1: Load Data" << std::endl;
    
    logai::DataLoaderConfig loader_config;
    loader_config.file_path = dataset_path;
    loader_config.log_type = "CSV"; // Already correct in code
    
    // Configure the log pattern for timestamp extraction
    loader_config.datetime_format = "%Y-%m-%d %H:%M:%S";
    loader_config.log_pattern = "^\\[(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})\\]";
    
    logai::FileDataLoader data_loader(loader_config);
    auto log_records = data_loader.load_data();
    
    std::cout << "Loaded " << log_records.size() << " log records." << std::endl;
    std::cout << "First 5 log records:" << std::endl;
    for (size_t i = 0; i < std::min<size_t>(5, log_records.size()); ++i) {
        std::cout << "  " << log_records[i].body << std::endl;
    }
    
    //=============================================================================
    // Step 2: Preprocess
    //=============================================================================
    std::cout << "\n## Step 2: Preprocess" << std::endl;
    
    // Convert log records to log lines for preprocessing
    std::vector<std::string> loglines;
    for (const auto& record : log_records) {
        loglines.push_back(record.body);
    }
    
    // Now the preprocessor config using the correct field names
    logai::PreprocessorConfig preprocessor_config;
    preprocessor_config.custom_delimiters_regex = {
        {"IP", "\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}"},
        {"UUID", "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"}
    };
    
    logai::Preprocessor preprocessor(preprocessor_config);
    
    // Process each log line individually since clean_log_line expects a single string_view
    std::vector<std::string> cleaned_log_texts;
    std::unordered_map<std::string, std::vector<std::vector<std::string>>> all_extracted_terms;
    
    for (const auto& logline : loglines) {
        auto [cleaned_text, extracted_terms] = preprocessor.clean_log_line(logline);
        cleaned_log_texts.push_back(cleaned_text);
        
        // Merge extracted terms into all_extracted_terms if needed
        // (implementation omitted for brevity)
    }
    
    //=============================================================================
    // Step 3: Parsing
    //=============================================================================
    std::cout << "\n## Step 3: Parsing" << std::endl;
    
    // Configure and create Drain parser
    int drain_depth = 5;
    double similarity_threshold = 0.5;
    int max_children = 100;
    
    logai::DrainParser parser(loader_config, drain_depth, similarity_threshold, max_children);
    
    // Parse the preprocessed logs
    std::vector<logai::LogRecordObject> parsed_records;
    std::vector<std::string> parsed_templates;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < cleaned_log_texts.size(); ++i) {
        auto parsed_record = parser.parse_line(cleaned_log_texts[i]);
        parsed_records.push_back(parsed_record);
        parsed_templates.push_back(parsed_record.template_str);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::cout << "Parsing completed in " << duration << " ms." << std::endl;
    std::cout << "Extracted template examples:" << std::endl;
    
    std::unordered_set<std::string> unique_templates;
    for (const auto& tmpl : parsed_templates) {
        unique_templates.insert(tmpl);
    }
    
    int count = 0;
    for (const auto& tmpl : unique_templates) {
        if (count++ >= 5) break;
        std::cout << "  " << tmpl << std::endl;
    }
    std::cout << "Found " << unique_templates.size() << " unique templates." << std::endl;
    
    //=============================================================================
    // Step 4: Feature Extraction for Time-series Analysis
    //=============================================================================
    std::cout << "\n## Step 4: Feature Extraction for Time-series Analysis" << std::endl;
    
    // Configure feature extractor for time-series analysis
    logai::FeatureExtractorConfig feature_config;
    feature_config.group_by_time = "15min";  // Group by 15-minute intervals
    feature_config.group_by_category = {"template"};  // Group by log template
    
    logai::FeatureExtractor feature_extractor(feature_config);
    
    // Convert parsed logs to counter vectors
    auto counter_vector_result = feature_extractor.convert_to_counter_vector(parsed_records);
    
    std::cout << "Created " << counter_vector_result.counts.size() << " time-series groups." << std::endl;
    std::cout << "Sample counts:" << std::endl;
    for (size_t i = 0; i < std::min(size_t(5), counter_vector_result.counts.size()); ++i) {
        std::cout << "  Group " << i << ": " << counter_vector_result.counts[i] << " events" << std::endl;
        
        // Print group identifiers (template and time)
        std::cout << "    Identifiers: ";
        for (const auto& [key, value] : counter_vector_result.group_identifiers[i]) {
            std::cout << key << "=" << value << " ";
        }
        std::cout << std::endl;
    }
    
    //=============================================================================
    // Step 5: Time-series Anomaly Detection
    //=============================================================================
    std::cout << "\n## Step 5: Time-series Anomaly Detection" << std::endl;
    
    // Convert counter vectors to Eigen matrix for anomaly detection
    int num_groups = counter_vector_result.counts.size();
    if (num_groups == 0) {
        std::cerr << "Error: No time-series groups found for anomaly detection." << std::endl;
        return 1;
    }
    
    // Create matrix with time and count columns
    Eigen::MatrixXd time_series_data(num_groups, 2);
    
    for (int i = 0; i < num_groups; ++i) {
        // Use index as time feature (simplified)
        time_series_data(i, 0) = i;
        // Use count as the value
        time_series_data(i, 1) = counter_vector_result.counts[i];
    }
    
    // Split data for training and testing
    auto [train_data, test_data] = train_test_split(time_series_data, 0.3);
    
    std::cout << "Training on " << train_data.rows() << " samples, testing on " 
              << test_data.rows() << " samples." << std::endl;
    
    // Use One-Class SVM for anomaly detection (similar to DBL in Python)
    logai::OneClassSVMParams svm_params;
    svm_params.kernel = "rbf";
    svm_params.nu = 0.1;  // Control the sensitivity (lower = more sensitive)
    
    logai::OneClassSVMDetector detector(svm_params);
    
    // Train the detector
    start_time = std::chrono::high_resolution_clock::now();
    detector.fit(train_data);
    
    // Predict anomalies
    Eigen::VectorXd anomaly_scores = detector.predict(test_data);
    end_time = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Anomaly detection completed in " << duration << " ms." << std::endl;
    
    // Count anomalies (score < 0 indicates anomaly)
    int anomaly_count = 0;
    for (int i = 0; i < anomaly_scores.size(); ++i) {
        if (anomaly_scores(i) < 0) {
            anomaly_count++;
        }
    }
    
    std::cout << "Found " << anomaly_count << " time-series anomalies out of " 
              << anomaly_scores.size() << " test samples." << std::endl;
    
    // Print some sample anomalies
    std::cout << "Sample time-series anomalies:" << std::endl;
    int samples_shown = 0;
    for (int i = 0; i < anomaly_scores.size() && samples_shown < 5; ++i) {
        if (anomaly_scores(i) < 0) {
            int original_idx = train_data.rows() + i;
            std::cout << "  Group with count " << test_data(i, 1) << " (index " << original_idx << ")" << std::endl;
            samples_shown++;
        }
    }
    
    //=============================================================================
    // Step 6: Vectorization for Semantic Anomaly Detection
    //=============================================================================
    std::cout << "\n## Step 6: Vectorization for Semantic Anomaly Detection" << std::endl;
    
    // Use LogBERT vectorizer (instead of Word2Vec in the Python example)
    logai::LogBERTVectorizerConfig vectorizer_config;
    vectorizer_config.max_token_len = 32;  // Use max_token_len instead of max_seq_length
    vectorizer_config.max_vocab_size = 5000;  // Use max_vocab_size instead of vocab_size
    
    // Create and train the vectorizer
    logai::LogBERTVectorizer vectorizer(vectorizer_config);
    
    start_time = std::chrono::high_resolution_clock::now();
    vectorizer.fit(parsed_templates);
    auto log_vectors = vectorizer.transform(parsed_templates);
    end_time = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Vectorization completed in " << duration << " ms." << std::endl;
    
    // Display sample vector dimensions
    std::cout << "Max sequence length: " << vectorizer_config.max_token_len << std::endl;
    
    //=============================================================================
    // Step 7: Categorical Encoding for Log Attributes
    //=============================================================================
    std::cout << "\n## Step 7: Categorical Encoding for Log Attributes" << std::endl;
    
    // Convert parsed records to arrow table
    auto attributes_table = log_records_to_arrow_table(parsed_records);
    
    // Create and apply label encoder
    logai::LabelEncoder encoder;
    auto encoded_attributes = encoder.fit_transform(attributes_table);
    
    std::cout << "Encoded " << encoded_attributes->num_columns() << " attribute columns." << std::endl;
    std::cout << "Total encoded records: " << encoded_attributes->num_rows() << std::endl;
    
    //=============================================================================
    // Step 8: Feature Extraction for Semantic Analysis
    //=============================================================================
    std::cout << "\n## Step 8: Feature Extraction for Semantic Analysis" << std::endl;
    
    // Configure feature extractor for semantic analysis (no time grouping)
    logai::FeatureExtractorConfig semantic_feature_config;
    semantic_feature_config.max_feature_len = 100; // Maximum feature length
    
    logai::FeatureExtractor semantic_feature_extractor(semantic_feature_config);
    
    // Convert log vectors and attributes to feature vectors
    // Create an arrow::Table from log_vectors
    auto log_vectors_table = log_records_to_arrow_table(parsed_records);
    
    // Pass correct parameters to convert_to_feature_vector
    auto feature_extraction_result = semantic_feature_extractor.convert_to_feature_vector(
        parsed_records, log_vectors_table);
    
    // Work with the feature extraction result
    std::cout << "Created feature vectors with " << 
        (feature_extraction_result.feature_vectors ? feature_extraction_result.feature_vectors->num_rows() : 0) 
        << " samples." << std::endl;
    
    //=============================================================================
    // Step 9: Semantic Anomaly Detection
    //=============================================================================
    std::cout << "\n## Step 9: Semantic Anomaly Detection" << std::endl;
    
    // Create matrix from feature vectors
    if (!feature_extraction_result.feature_vectors || feature_extraction_result.feature_vectors->num_rows() == 0) {
        std::cerr << "Error: No feature vectors generated from feature extraction." << std::endl;
        return 1;
    }

    int num_samples = feature_extraction_result.feature_vectors->num_rows();
    int num_features = feature_extraction_result.feature_vectors->num_columns();

    std::cout << "Feature matrix dimensions: " << num_samples << " x " << num_features << std::endl;

    Eigen::MatrixXd features(num_samples, num_features);
    // Extract data from Arrow table
    try {
        for (int i = 0; i < num_samples; ++i) {
            for (int j = 0; j < num_features; ++j) {
                // Access data from Arrow table columns - assuming column type is double
                auto column = feature_extraction_result.feature_vectors->column(j);
                
                if (column->num_chunks() == 0) {
                    features(i, j) = 0.0;
                    continue;
                }
                
                auto chunk = column->chunk(0);  // Assuming single chunk per column
                
                if (chunk->type_id() == arrow::Type::DOUBLE) {
                    auto double_array = std::static_pointer_cast<arrow::DoubleArray>(chunk);
                    features(i, j) = double_array->Value(i);
                } else if (chunk->type_id() == arrow::Type::INT64) {
                    auto int_array = std::static_pointer_cast<arrow::Int64Array>(chunk);
                    features(i, j) = static_cast<double>(int_array->Value(i));
                } else {
                    // Default to zero if type is not as expected
                    features(i, j) = 0.0;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error extracting features from Arrow table: " << e.what() << std::endl;
        return 1;
    }
    
    // Split data for training and testing
    auto [semantic_train, semantic_test] = train_test_split(features, 0.7);
    
    std::cout << "Training on " << semantic_train.rows() << " samples, testing on " 
              << semantic_test.rows() << " samples." << std::endl;
    
    // Use One-Class SVM for semantic anomaly detection (similar to isolation_forest in Python)
    logai::OneClassSVMParams semantic_svm_params;
    semantic_svm_params.kernel = "rbf";
    semantic_svm_params.nu = 0.05;  // More restrictive threshold for isolation
    
    logai::OneClassSVMDetector semantic_detector(semantic_svm_params);
    
    // Train the detector
    start_time = std::chrono::high_resolution_clock::now();
    semantic_detector.fit(semantic_train);
    
    // Predict anomalies
    Eigen::VectorXd semantic_anomaly_scores = semantic_detector.predict(semantic_test);
    end_time = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Semantic anomaly detection completed in " << duration << " ms." << std::endl;
    
    // Count anomalies (score < 0 indicates anomaly)
    int semantic_anomaly_count = 0;
    for (int i = 0; i < semantic_anomaly_scores.size(); ++i) {
        if (semantic_anomaly_scores(i) < 0) {
            semantic_anomaly_count++;
        }
    }
    
    std::cout << "Found " << semantic_anomaly_count << " semantic anomalies out of " 
              << semantic_anomaly_scores.size() << " test samples." << std::endl;
    
    // Print some sample semantic anomalies with their corresponding log lines
    std::cout << "Sample semantic anomalies:" << std::endl;
    samples_shown = 0;
    for (int i = 0; i < semantic_anomaly_scores.size() && samples_shown < 5; ++i) {
        if (semantic_anomaly_scores(i) < 0) {
            int original_idx = semantic_train.rows() + i;
            std::cout << "  Log template: " << parsed_templates[original_idx] << std::endl;
            std::cout << "  Original log: " << log_records[original_idx].body << std::endl;
            std::cout << std::endl;
            samples_shown++;
        }
    }
    
    //=============================================================================
    // Alternative: DBSCAN Clustering for Anomaly Detection
    //=============================================================================
    std::cout << "\n## Alternative: DBSCAN Clustering for Anomaly Detection" << std::endl;
    
    // Update DBSCAN parameters with correct field names
    logai::DbScanParams dbscan_params;
    dbscan_params.eps = 0.5; // Epsilon radius for neighborhood
    
    logai::DbScanClustering dbscan(dbscan_params);
    
    start_time = std::chrono::high_resolution_clock::now();
    // Use a subset of features for demonstration
    Eigen::MatrixXd subset_features = features.topRows(std::min(1000, static_cast<int>(features.rows())));
    
    // Convert Eigen matrix to vector of vectors for DBSCAN
    std::vector<std::vector<float>> feature_vectors_vec;
    for (int i = 0; i < subset_features.rows(); ++i) {
        std::vector<float> row;
        for (int j = 0; j < subset_features.cols(); ++j) {
            row.push_back(static_cast<float>(subset_features(i, j)));
        }
        feature_vectors_vec.push_back(row);
    }
    
    dbscan.fit(feature_vectors_vec);
    auto cluster_labels = dbscan.get_labels();
    end_time = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "DBSCAN clustering completed in " << duration << " ms." << std::endl;
    
    // Count points in each cluster
    std::unordered_map<int, int> cluster_counts;
    for (int label : cluster_labels) {
        cluster_counts[label]++;
    }
    
    std::cout << "DBSCAN clustering results:" << std::endl;
    for (const auto& [cluster, count] : cluster_counts) {
        std::cout << "  Cluster " << cluster << ": " << count << " points";
        if (cluster == -1) {
            std::cout << " (noise/anomalies)";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\nLog Anomaly Detection demonstration completed successfully!" << std::endl;
    
    return 0;
}

// Helper function to convert log records to an Arrow table
std::shared_ptr<arrow::Table> log_records_to_arrow_table(
    const std::vector<logai::LogRecordObject>& records) {
    
    arrow::StringBuilder content_builder;
    
    // Reserve space for all elements
    auto status = content_builder.Reserve(records.size());
    if (!status.ok()) {
        std::cerr << "Failed to reserve memory for content builder: " << status.ToString() << std::endl;
        return nullptr;
    }
    
    // Create arrays from records
    for (const auto& record : records) {
        status = content_builder.Append(record.body);
        if (!status.ok()) {
            std::cerr << "Failed to append to content builder: " << status.ToString() << std::endl;
            return nullptr;
        }
    }
    
    // Finish building arrays
    std::shared_ptr<arrow::StringArray> content_array;
    status = content_builder.Finish(&content_array);
    if (!status.ok()) {
        std::cerr << "Failed to finish content builder: " << status.ToString() << std::endl;
        return nullptr;
    }
    
    // Create schema
    auto field_content = arrow::field("content", arrow::utf8());
    auto schema = arrow::schema({field_content});
    
    // Create table
    return arrow::Table::Make(schema, {content_array});
}

// Helper function to convert token sequences to matrix
Eigen::MatrixXd convert_tokens_to_matrix(
    const std::vector<std::vector<int>>& token_sequences, 
    int max_features) {
    size_t num_samples = token_sequences.size();
    
    // Create feature matrix
    Eigen::MatrixXd feature_matrix(num_samples, max_features);
    
    // Fill matrix with token ids (padding or truncating as needed)
    for (size_t i = 0; i < num_samples; ++i) {
        const auto& tokens = token_sequences[i];
        for (int j = 0; j < max_features; ++j) {
            if (j < static_cast<int>(tokens.size())) {
                feature_matrix(i, j) = tokens[j];
            } else {
                feature_matrix(i, j) = 0;  // Padding
            }
        }
    }
    
    return feature_matrix;
} 