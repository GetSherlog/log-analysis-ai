#include "label_encoder.h"
#include <iostream>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/csv/api.h>
#include <arrow/dataset/api.h>
#include <arrow/builder.h>
#include <arrow/pretty_print.h>

using namespace logai;

// Helper function to create a test table
std::shared_ptr<arrow::Table> create_test_table() {
    // Create builders for our columns
    arrow::StringBuilder category_builder;
    arrow::StringBuilder value_builder;
    
    // Add data to the category column
    auto status = category_builder.AppendValues({
        "A", "B", "C", "A", "B", "A", "C", "B", "C", "A"
    });
    if (!status.ok()) {
        throw std::runtime_error("Failed to append category values: " + status.ToString());
    }
    
    // Add data to the value column
    status = value_builder.AppendValues({
        "apple", "banana", "cherry", "apple", "banana", 
        "avocado", "cherry", "blueberry", "cherry", "apple"
    });
    if (!status.ok()) {
        throw std::runtime_error("Failed to append value values: " + status.ToString());
    }
    
    // Finish building arrays
    std::shared_ptr<arrow::Array> category_array;
    std::shared_ptr<arrow::Array> value_array;
    status = category_builder.Finish(&category_array);
    if (!status.ok()) {
        throw std::runtime_error("Failed to finish category array: " + status.ToString());
    }
    
    status = value_builder.Finish(&value_array);
    if (!status.ok()) {
        throw std::runtime_error("Failed to finish value array: " + status.ToString());
    }
    
    // Create table schema
    auto schema = arrow::schema({
        arrow::field("category", arrow::utf8()),
        arrow::field("value", arrow::utf8())
    });
    
    // Create table
    return arrow::Table::Make(schema, {category_array, value_array});
}

void print_table(const std::shared_ptr<arrow::Table>& table) {
    std::cout << "Table contents:\n";
    arrow::PrettyPrintOptions options;
    // Fix: n_rows doesn't exist in PrettyPrintOptions
    // Use the correct property or don't set it to show all rows
    
    // Call PrettyPrint and check its status
    auto status = arrow::PrettyPrint(*table, options, &std::cout);
    if (!status.ok()) {
        std::cerr << "Error printing table: " << status.ToString() << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    try {
        // Create test data
        auto test_table = create_test_table();
        std::cout << "Original table:\n";
        print_table(test_table);
        
        // Create and fit label encoder
        LabelEncoder encoder;
        auto encoded_table = encoder.fit_transform(test_table);
        
        std::cout << "Encoded table:\n";
        print_table(encoded_table);
        
        // Get classes for each column
        std::cout << "\nClasses for 'category':\n";
        auto category_classes = encoder.get_classes("category");
        for (size_t i = 0; i < category_classes.size(); ++i) {
            std::cout << i << " -> " << category_classes[i] << std::endl;
        }
        
        std::cout << "\nClasses for 'value':\n";
        auto value_classes = encoder.get_classes("value");
        for (size_t i = 0; i < value_classes.size(); ++i) {
            std::cout << i << " -> " << value_classes[i] << std::endl;
        }
        
        // Create a new table to test transform
        arrow::StringBuilder new_category_builder;
        auto status = new_category_builder.AppendValues({
            "A", "B", "C", "D"  // 'D' is a new value not seen during fitting
        });
        if (!status.ok()) {
            throw std::runtime_error("Failed to append new category values: " + status.ToString());
        }
        
        std::shared_ptr<arrow::Array> new_category_array;
        status = new_category_builder.Finish(&new_category_array);
        if (!status.ok()) {
            throw std::runtime_error("Failed to finish new category array: " + status.ToString());
        }
        
        auto new_schema = arrow::schema({
            arrow::field("category", arrow::utf8())
        });
        
        auto new_table = arrow::Table::Make(new_schema, {new_category_array});
        
        std::cout << "\nNew table to transform:\n";
        print_table(new_table);
        
        // Transform the new table
        auto transformed_table = encoder.transform(new_table);
        
        std::cout << "Transformed table (note the null for unseen 'D' value):\n";
        print_table(transformed_table);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 