#pragma once

// This header includes all standard library headers with proper namespace handling

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <filesystem>

// Ensure all std types are properly qualified
namespace std {
    // Explicitly bring all std types into the std namespace
    using std::string;
    using std::vector;
    using std::unique_ptr;
    using std::make_unique;
    using std::thread;
    using std::function;
    using std::ifstream;
    using std::ofstream;
    using std::string_view;
    using std::cerr;
    using std::endl;
    using std::runtime_error;
    using std::move;
    using std::atomic;
    using std::ios_base;
    using std::basic_streambuf;
    using std::streamsize;
    using std::locale;
    using std::codecvt;
    using std::ostream;
}

// Define namespace alias for filesystem
namespace fs = std::filesystem; 