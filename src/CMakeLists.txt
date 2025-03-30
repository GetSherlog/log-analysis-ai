# Source files (all files in the current directory, excluding test and benchmark files)
file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

# Create library
add_library(logai
    file_data_loader.cpp
    file_data_loader.h
    log_parser.h
    logfmt_parser.cpp
    syslog_parser.cpp
    log4j_parser.cpp
    cef_parser.cpp
    line_parser.cpp
)
target_include_directories(logai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Find required packages
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Link with dependencies
target_link_libraries(logai 
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE ${CURL_LIBRARIES}
    PRIVATE folly
    PRIVATE Boost::iostreams
    PRIVATE Boost::regex
    PRIVATE Boost::algorithm
)

# Add pybind11
find_package(pybind11 REQUIRED)

# Python bindings
pybind11_add_module(logai_cpp python_bindings.cpp)
target_link_libraries(logai_cpp PRIVATE 
    logai
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    spdlog
    Boost::iostreams  
    Boost::regex
    Boost::algorithm
    folly
)

# Install
install(TARGETS logai
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(FILES ${HEADERS} DESTINATION include/logai) 