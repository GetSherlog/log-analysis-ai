#!/usr/bin/env python3
import os
import sys
import glob
import shutil
import platform
import subprocess
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext

# Get the absolute path to the project root
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_DIR = os.path.join(PROJECT_ROOT, "src")
BUILD_DIR = os.path.join(PROJECT_ROOT, "build")

# Check if we need to build or if we already have a built extension
PREBUILD_EXTENSION = False
extension_path = None

# Look for the built extension in current directory
for ext_file in glob.glob("logai_cpp*.so"):
    extension_path = ext_file
    PREBUILD_EXTENSION = True
    break

# If not found, check the build directory
if not PREBUILD_EXTENSION:
    for ext_file in glob.glob(os.path.join(BUILD_DIR, "logai_cpp*.so")):
        extension_path = ext_file
        # Copy to current directory
        shutil.copy2(extension_path, ".")
        extension_path = os.path.basename(extension_path)
        PREBUILD_EXTENSION = True
        break

class CMakeExtension(Extension):
    def __init__(self, name, cmake_lists_dir='.', **kwa):
        Extension.__init__(self, name, sources=[], **kwa)
        self.cmake_lists_dir = os.path.abspath(cmake_lists_dir)

class CMakeBuild(build_ext):
    def build_extension(self, ext):
        # Skip if we have a pre-built extension
        if PREBUILD_EXTENSION:
            print(f"Using pre-built extension: {extension_path}")
            return
            
        ext_dir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        
        # Required for auto-detection of auxiliary "native" libs
        if not ext_dir.endswith(os.path.sep):
            ext_dir += os.path.sep

        cmake_args = [
            f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={ext_dir}',
            f'-DPYTHON_EXECUTABLE={sys.executable}',
            f'-DCMAKE_BUILD_TYPE=Release',
        ]

        build_args = [
            '--config', 'Release',
            '--parallel', '4'
        ]

        os.makedirs(self.build_temp, exist_ok=True)
        
        # Build the entire C++ project first
        subprocess.check_call(['cmake', ext.cmake_lists_dir] + cmake_args, 
                              cwd=self.build_temp)
        subprocess.check_call(['cmake', '--build', '.'] + build_args, 
                             cwd=self.build_temp)

# Setup package data to include the extension
package_data = {}
if PREBUILD_EXTENSION:
    # Include the extension file in package data
    package_data = {
        '': [extension_path] if extension_path else [],
    }

setup(
    name="logai-cpp",
    version="0.1.0",
    author="LogAI Team",
    author_email="info@logai.com",
    description="Python bindings for LogAI C++ library",
    long_description="",
    ext_modules=[] if PREBUILD_EXTENSION else [CMakeExtension("logai_cpp", PROJECT_ROOT)],
    cmdclass={"build_ext": CMakeBuild},
    packages=find_packages(),
    package_data=package_data,
    include_package_data=True,
    install_requires=[
        "instructor>=0.5.0",
        "openai>=1.0.0",
        "pydantic>=2.0.0",
        "rich>=13.0.0",
        "pybind11>=2.10.0",
        "duckdb>=0.10.0",
        "qdrant-client>=1.13.3",
        "python-dotenv>=1.0.0",
        "tqdm>=4.66.0",
        "numpy>=1.24.0",
        "pandas>=2.0.0",
        "pydantic-ai==0.0.46",
        "anthropic>=0.8.0",  # For Claude model
        "google-generativeai>=0.3.0",  # For Gemini model
        "scikit-learn>=1.3.0",  # For statistical analysis
        "scipy>=1.11.0",  # For scientific computing
        "networkx>=3.1",  # For dependency graph analysis
        "matplotlib>=3.7.0",  # For visualization
        "seaborn>=0.12.0",  # For statistical visualizations
        "plotly>=5.18.0",  # For interactive visualizations
    ],
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.8",
    entry_points={
        "console_scripts": [
            "logai-agent=logai_agent:main",
        ],
    },
) 