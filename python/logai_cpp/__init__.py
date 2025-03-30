"""
LogAI Python Package

A hybrid package that imports functionality from both:
1. The C++ extension module (via pybind11)
2. Pure Python implementations
"""
import logging
import sys
import os
import importlib.util
import importlib
from typing import List, Dict, Any, Optional

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Define default implementations
parse_log_file = None
process_large_file_with_callback = None
extract_attributes = None

# Try to import the C++ module first
try:
    # Get the directory of the current package
    package_dir = os.path.dirname(__file__)
    extension_path = None
    
    # First, check if the extension is in the current directory
    for file in os.listdir(package_dir):
        if file.startswith("logai_cpp") and file.endswith(".so"):
            extension_path = os.path.join(package_dir, file)
            break
    
    # If not found, look in the parent directory
    if not extension_path:
        parent_dir = os.path.dirname(package_dir)
        for file in os.listdir(parent_dir):
            if file.startswith("logai_cpp") and file.endswith(".so"):
                extension_path = os.path.join(parent_dir, file)
                break
    
    # If still not found, try to look in the build directory
    if not extension_path:
        build_dir = os.path.join(os.path.dirname(os.path.dirname(package_dir)), "build")
        if os.path.exists(build_dir):
            for file in os.listdir(build_dir):
                if file.startswith("logai_cpp") and file.endswith(".so"):
                    extension_path = os.path.join(build_dir, file)
                    break
    
    if extension_path:
        spec = importlib.util.spec_from_file_location("_logai_cpp", extension_path)
        if spec and spec.loader:
            module = importlib.util.module_from_spec(spec)
            sys.modules["_logai_cpp"] = module
            spec.loader.exec_module(module)
            
            # Import the functions we want to keep from C++
            try:
                # Use getattr to access functions from the dynamically loaded module
                parse_log_file = getattr(module, "parse_log_file")
                process_large_file_with_callback = getattr(module, "process_large_file_with_callback")
                extract_attributes = getattr(module, "extract_attributes")
                
                # Log successful import
                logger.info(f"Successfully loaded LogAI C++ extension from {extension_path}")
            except AttributeError as e:
                logger.warning(f"Could not find functions in the extension: {str(e)}")
        else:
            logger.warning("Failed to create module spec from the extension file")
    else:
        logger.warning("LogAI C++ extension not found in any directory")
except Exception as e:
    logger.warning(f"Failed to import LogAI C++ extension: {str(e)}")

# Import Python implementations
from .embeddings import generate_template_embedding, GeminiVectorizer
from .milvus_client import (
    init_milvus,
    get_milvus_connection_string,
    insert_template,
    search_similar_templates
)

# Define what should be accessible when importing the package
__all__ = [
    # C++ functions
    "parse_log_file",
    "process_large_file_with_callback",
    "extract_attributes",
    
    # Python implementations for embeddings
    "generate_template_embedding",
    "GeminiVectorizer",
    
    # Python implementations for Milvus
    "init_milvus",
    "get_milvus_connection_string",
    "insert_template",
    "search_similar_templates"
] 