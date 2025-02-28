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
    ARROW_RETURN_NOT_OK(category_builder.AppendValues({
        "A", "B", "C", "A", "B", "A", "C", "B", "C", "A"
    }));
    
    // Add data to the value column
    ARROW_RETURN_NOT_OK(value_builder.AppendValues({
        "apple", "banana", "cherry", "apple", "banana", 
        "avocado", "cherry", "blueberry", "cherry", "apple"
    }));
    
    // Finish building arrays
    std::shared_ptr<arrow::Array> category_array;
    std::shared_ptr<arrow::Array> value_array;
    ARROW_RETURN_NOT_OK(category_builder.Finish(&category_array));
    ARROW_RETURN_NOT_OK(value_builder.Finish(&value_array));
    
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
    options.n_rows = table->num_rows();
    arrow::PrettyPrint(*table, options, &std::cout);
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
        ARROW_RETURN_NOT_OK(new_category_builder.AppendValues({
            "A", "B", "C", "D"  // 'D' is a new value not seen during fitting
        }));
        
        std::shared_ptr<arrow::Array> new_category_array;
        ARROW_RETURN_NOT_OK(new_category_builder.Finish(&new_category_array));
        
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