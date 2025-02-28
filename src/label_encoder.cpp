/**
 * @file label_encoder.cpp
 * @brief Implementation of the label encoder for categorical data
 */

#include "label_encoder.h"
#include <algorithm>
#include <stdexcept>
#include <arrow/array.h>
#include <arrow/builder.h>
#include <arrow/compute/api.h>
#include <arrow/type.h>
#include <arrow/util/checked_cast.h>

namespace logai {

LabelEncoder::LabelEncoder() : is_fitted_(false) {}

std::shared_ptr<arrow::Table> LabelEncoder::fit_transform(const std::shared_ptr<arrow::Table>& table) {
    // Reset the encoder state
    column_mappings_.clear();
    is_fitted_ = false;
    
    // Create vectors to hold the encoded arrays and their fields
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    // Process each column in the table
    for (int i = 0; i < table->num_columns(); ++i) {
        const auto& column = table->column(i);
        const auto& field = table->field(i);
        
        // Only process string columns
        if (field->type()->id() == arrow::Type::STRING) {
            // Cast the column to a StringArray
            auto string_array = std::static_pointer_cast<arrow::StringArray>(column->chunk(0));
            
            // Encode the column (and fit the encoder)
            auto encoded_array = encode_column(field->name(), string_array, true);
            
            // Add the encoded column to results
            std::string encoded_name = field->name() + "_categorical";
            fields.push_back(arrow::field(encoded_name, arrow::int32()));
            arrays.push_back(encoded_array);
        }
    }
    
    // Mark encoder as fitted
    is_fitted_ = true;
    
    // Return a new table with the encoded columns
    return arrow::Table::Make(arrow::schema(fields), arrays);
}

std::shared_ptr<arrow::Table> LabelEncoder::transform(const std::shared_ptr<arrow::Table>& table) const {
    if (!is_fitted_) {
        throw std::runtime_error("Encoder must be fitted before transform can be called");
    }
    
    // Create vectors to hold the encoded arrays and their fields
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    // Process each column in the table
    for (int i = 0; i < table->num_columns(); ++i) {
        const auto& column = table->column(i);
        const auto& field = table->field(i);
        
        // Only process string columns that have been fitted
        if (field->type()->id() == arrow::Type::STRING && 
            column_mappings_.find(field->name()) != column_mappings_.end()) {
            
            // Cast the column to a StringArray
            auto string_array = std::static_pointer_cast<arrow::StringArray>(column->chunk(0));
            
            // Encode the column (without fitting)
            auto encoded_array = encode_column(field->name(), string_array, false);
            
            // Add the encoded column to results
            std::string encoded_name = field->name() + "_categorical";
            fields.push_back(arrow::field(encoded_name, arrow::int32()));
            arrays.push_back(encoded_array);
        }
    }
    
    // Return a new table with the encoded columns
    return arrow::Table::Make(arrow::schema(fields), arrays);
}

bool LabelEncoder::is_fitted() const {
    return is_fitted_;
}

std::vector<std::string> LabelEncoder::get_classes(const std::string& column_name) const {
    if (!is_fitted_) {
        throw std::runtime_error("Encoder must be fitted before get_classes can be called");
    }
    
    auto it = column_mappings_.find(column_name);
    if (it == column_mappings_.end()) {
        throw std::runtime_error("Column '" + column_name + "' not found in encoder");
    }
    
    // Create a vector with size equal to the number of classes
    std::vector<std::string> classes(it->second.size());
    
    // Populate the vector based on the index mapping
    for (const auto& [value, index] : it->second) {
        classes[index] = value;
    }
    
    return classes;
}

std::shared_ptr<arrow::Array> LabelEncoder::encode_column(
    const std::string& column_name,
    const std::shared_ptr<arrow::StringArray>& input_column,
    bool fit) const {
    
    // Get or create mapping for this column
    std::unordered_map<std::string, int> mapping;
    if (fit) {
        // If fitting, create a new mapping
        auto& mutable_self = const_cast<LabelEncoder&>(*this);
        auto& mutable_mapping = mutable_self.column_mappings_[column_name];
        
        // First pass: identify unique values and assign indices
        for (int64_t i = 0; i < input_column->length(); ++i) {
            if (!input_column->IsNull(i)) {
                std::string value = input_column->GetString(i);
                if (mutable_mapping.find(value) == mutable_mapping.end()) {
                    mutable_mapping[value] = static_cast<int>(mutable_mapping.size());
                }
            }
        }
        
        mapping = mutable_mapping;
    } else {
        // Use existing mapping if available
        auto it = column_mappings_.find(column_name);
        if (it != column_mappings_.end()) {
            mapping = it->second;
        }
    }
    
    // Reserve space for all elements
    arrow::Int32Builder builder;
    auto status = builder.Reserve(input_column->length());
    if (!status.ok()) {
        return nullptr;
    }
    
    // Encode the values
    for (int64_t i = 0; i < input_column->length(); ++i) {
        if (input_column->IsNull(i)) {
            status = builder.AppendNull();
            if (!status.ok()) {
                return nullptr;
            }
        } else {
            auto value = input_column->GetString(i);
            auto it = mapping.find(value);
            if (it != mapping.end()) {
                status = builder.Append(it->second);
                if (!status.ok()) {
                    return nullptr;
                }
            } else {
                status = builder.AppendNull();
                if (!status.ok()) {
                    return nullptr;
                }
            }
        }
    }
    
    // Finalize the array
    std::shared_ptr<arrow::Array> result;
    status = builder.Finish(&result);
    if (!status.ok()) {
        return nullptr;
    }
    
    return result;
}

} // namespace logai 