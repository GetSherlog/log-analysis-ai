#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <filesystem>
#include <unordered_set>
#include <optional>
#include <arrow/api.h>
#include <arrow/status.h>
#include "data_loader_config.h"
#include "log_record.h"
#include "memory_mapped_file.h"
#include "thread_safe_queue.h"
#include "log_parser.h"

// Forward declaration for Arrow Table
namespace arrow {
    class Table;
    class Status;
}

namespace logai {

/**
 * @brief Batch of log lines for parallel processing
 */
struct LogBatch {
    size_t id;
    std::vector<std::string> lines;
};

/**
 * @brief Processed batch of log data
 */
struct ProcessedBatch {
    size_t id;
    std::vector<LogRecordObject> records;
};

/**
 * @brief High-performance file data loader for log analysis
 */
class FileDataLoader {
public:
    explicit FileDataLoader(const DataLoaderConfig& config);
    ~FileDataLoader();

    std::vector<LogRecordObject> load_data();
    double get_progress() const;
    
    // New Arrow-based operations
    std::shared_ptr<arrow::Table> log_to_dataframe(const std::string& filepath, const std::string& format);
    std::shared_ptr<arrow::Table> filter_dataframe(std::shared_ptr<arrow::Table> table, const std::vector<std::string>& dimensions);
    std::shared_ptr<arrow::Table> filter_dataframe(std::shared_ptr<arrow::Table> table, 
                                                  const std::string& column, 
                                                  const std::string& op, 
                                                  const std::string& value);
    arrow::Status write_to_parquet(std::shared_ptr<arrow::Table> table, const std::string& output_path);

    // Process a single batch of log lines
    void process_batch(const LogBatch& batch, ProcessedBatch& result);

private:
    DataLoaderConfig config_;
    std::atomic<size_t> total_lines_read_{0};
    std::atomic<size_t> processed_lines_{0};
    std::atomic<size_t> failed_lines_{0};
    std::atomic<bool> running_{false};
    std::atomic<double> progress_{0.0};
    std::atomic<size_t> total_batches_{0};
    
    // Multi-threading components
    ThreadSafeQueue<LogBatch> batch_queue_;
    ThreadSafeQueue<ProcessedBatch> processed_queue_;
    std::vector<std::thread> worker_threads_;
    
    std::vector<LogRecordObject> read_logs(const std::string& filepath);
    std::vector<LogRecordObject> read_csv(const std::string& filepath);
    std::vector<LogRecordObject> read_tsv(const std::string& filepath);
    std::vector<LogRecordObject> read_json(const std::string& filepath);
    
    std::vector<LogRecordObject> create_log_record_objects(
        const std::vector<std::vector<std::string>>& data, 
        const std::vector<std::string>& headers);
    
    std::vector<std::string> simd_parse_csv_line(const std::string& line, char delimiter);
    bool simd_pattern_search(const std::string& line, const std::string& pattern);
    
    void read_file_by_chunks(
        const std::string& filepath, 
        const std::function<void(const std::string&)>& callback);
    
    void read_file_memory_mapped(
        const std::string& filepath,
        std::function<void(std::string_view)> callback);
    
    void reader_thread(const std::string& filepath);
    void worker_thread(ThreadSafeQueue<LogBatch>& input_queue, ThreadSafeQueue<ProcessedBatch>& output_queue);
    void collector_thread();
    
    ProcessedBatch process_batch(const LogBatch& batch, const std::string& log_format = "");
    std::unique_ptr<LogParser> create_parser();
    void producer_thread(MemoryMappedFile& file, ThreadSafeQueue<LogBatch>& input_queue, std::atomic<size_t>& total_batches);
    void consumer_thread(size_t num_threads, ThreadSafeQueue<ProcessedBatch>& output_queue, std::vector<LogRecordObject>& results, std::atomic<size_t>& total_batches);

    // Read file line by line with callback
    void read_file_line_by_line(const std::string& filepath, 
                               std::function<void(std::string_view)> callback);
};

} 