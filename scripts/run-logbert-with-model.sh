#!/bin/bash

# Exit on error
set -e

# Define variables
OUTPUT_DIR="./bert_model"
TOKENIZER_MODEL="./test_data/tokenizer_model.json"
SAMPLE_LOG_FILE="./test_data/sample_logs.txt"

# Create output directory
mkdir -p "$OUTPUT_DIR"
mkdir -p "test_data"

echo "LogBERT with Pre-trained BERT Model Example"
echo "==========================================="

# Check if sample logs exist, if not create a sample
if [ ! -f "$SAMPLE_LOG_FILE" ]; then
    echo "Creating sample log file..."
    cat > "$SAMPLE_LOG_FILE" << EOL
2023-01-15T12:34:56.789Z INFO [app.server] Server started at port 8080
2023-01-15T12:35:01.123Z DEBUG [app.connection] Client connected from 192.168.1.100
2023-01-15T12:35:02.456Z INFO [app.request] GET /api/users HTTP/1.1
2023-01-15T12:35:02.789Z DEBUG [app.db] Query executed in 54ms: SELECT * FROM users LIMIT 100
2023-01-15T12:35:03.012Z ERROR [app.handler] Failed to process request: Invalid user ID format
EOL
    echo "Created sample log file with 5 entries."
fi

# Build the project
echo "Building LogBERT test program..."
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8 logbert_test
cd ..

# Run the LogBERT test to tokenize the logs
echo -e "\nTokenizing log entries..."
./build/src/logbert_test "$SAMPLE_LOG_FILE" "$TOKENIZER_MODEL"

# Download BERT model
echo -e "\nChecking for pre-trained BERT model..."
if [ ! -d "$OUTPUT_DIR/bert-base-uncased" ]; then
    echo "Downloading pre-trained BERT model using Python scripts..."
    cat > download_bert.py << EOL
import os
from transformers import BertModel, BertTokenizer, BertConfig

# Create directory
os.makedirs("${OUTPUT_DIR}", exist_ok=True)

# Download model and tokenizer
model_name = "bert-base-uncased"
tokenizer = BertTokenizer.from_pretrained(model_name)
model = BertModel.from_pretrained(model_name)

# Save to disk
tokenizer.save_pretrained("${OUTPUT_DIR}/" + model_name)
model.save_pretrained("${OUTPUT_DIR}/" + model_name)
config = BertConfig.from_pretrained(model_name)
config.save_pretrained("${OUTPUT_DIR}/" + model_name)

print("Downloaded and saved BERT model to ${OUTPUT_DIR}/" + model_name)
EOL

    # Check if pip and transformers are installed
    if ! command -v pip &> /dev/null; then
        echo "ERROR: pip is not installed. Please install Python and pip first."
        exit 1
    fi
    
    # Install transformers if needed
    if ! pip show transformers &> /dev/null; then
        echo "Installing transformers library..."
        pip install transformers torch
    fi
    
    # Run the download script
    python download_bert.py
else
    echo "BERT model already downloaded at $OUTPUT_DIR/bert-base-uncased"
fi

# Create an example script that shows how to use our tokenizer with BERT model
echo -e "\nCreating example script to use tokenized logs with BERT model..."
cat > process_logs_with_bert.py << EOL
import os
import json
import torch
from transformers import BertModel

# Path to BERT model
model_path = "${OUTPUT_DIR}/bert-base-uncased"
bert_model = BertModel.from_pretrained(model_path)

# Load tokenized data 
# (This would normally come from your C++ code via some serialization mechanism)
# For demo purposes, we'll generate fake tokenized data
def create_dummy_tokenized_data():
    # Simulate the structure that would come from LogBERTVectorizer::transform_with_attention
    # For a real implementation, you would need to serialize the C++ output to a file
    # and read it here
    return [
        {
            "token_ids": [101, 2023, 2015, 1012, 3446, 2015, 2090, 2339, 1024, 2035, 1012, 2339, 1013, 102] + [0] * 50,
            "attention_mask": [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1] + [0] * 50
        },
        {
            "token_ids": [101, 2023, 2015, 1012, 3446, 2059, 2015, 2131, 1024, 2035, 1012, 2339, 1013, 102] + [0] * 50,
            "attention_mask": [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1] + [0] * 50
        }
    ]

# Get tokenized data
tokenized_data = create_dummy_tokenized_data()

# Process through BERT model
print("Processing tokenized logs through BERT model...\n")
for i, example in enumerate(tokenized_data):
    # Create input tensors
    input_ids = torch.tensor([example["token_ids"]], dtype=torch.long)
    attention_mask = torch.tensor([example["attention_mask"]], dtype=torch.long)
    
    # Process through BERT (no gradients needed for inference)
    with torch.no_grad():
        outputs = bert_model(input_ids=input_ids, attention_mask=attention_mask)
    
    # Get the embeddings
    last_hidden_state = outputs.last_hidden_state
    cls_embedding = last_hidden_state[:, 0, :]  # Get the [CLS] token embedding
    
    print(f"Log entry {i+1}:")
    print(f"  Input shape: {input_ids.shape}")
    print(f"  Output embeddings shape: {last_hidden_state.shape}")
    print(f"  CLS embedding shape: {cls_embedding.shape}")
    print(f"  CLS embedding (first 5 values): {cls_embedding[0, :5].numpy()}\n")

print("Note: In a real application, you would:")
print("1. Convert C++ token IDs and attention masks to a format readable by Python")
print("2. Use more sophisticated methods to feed data between your C++ LogBERT and the Python BERT model")
print("3. Consider implementing the BERT model itself in C++ using ONNX or TensorRT for a pure C++ solution")
EOL

echo -e "\nExample ready! To see how to use our tokenizer with BERT, run:"
echo "python process_logs_with_bert.py"
echo -e "\nThis demonstrates how to take token IDs generated by our C++ LogBERT vectorizer"
echo "and use them with a pre-trained BERT model in Python."

# Optional: Run the example if pytorch and transformers are installed
if pip show torch transformers &> /dev/null; then
    echo -e "\nRunning the example..."
    python process_logs_with_bert.py
else
    echo -e "\nTo run the example, install the required Python libraries:"
    echo "pip install torch transformers"
fi 