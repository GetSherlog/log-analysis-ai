# LogAI Web Server CMakeLists.txt

# Set minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Source files
set(SERVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/LogAIServer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/controllers/AnomalyDetectionController.cpp"
)

# Headers
set(SERVER_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/controllers/ApiController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/controllers/AnomalyDetectionController.h"
)

# Find required packages
# Only find Drogon if it hasn't been found yet
if(NOT TARGET Drogon::Drogon)
    find_package(Drogon REQUIRED)
endif()

# Find Abseil
find_package(absl CONFIG REQUIRED)

# Find Eigen
find_package(Eigen3 REQUIRED)

# Create the web server executable
add_executable(logai_web_server ${SERVER_SOURCES} ${SERVER_HEADERS})

# Set the output directory to bin
set_target_properties(logai_web_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(logai_web_server 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${DROGON_INCLUDE_DIRS}
)

# Link the libraries
target_link_libraries(logai_web_server 
    PRIVATE ${DROGON_LIBRARIES}
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE Eigen3::Eigen
)

# Installation
install(TARGETS logai_web_server 
    RUNTIME DESTINATION bin
)

# Add optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(logai_web_server PRIVATE -O3 -march=native)
elseif(MSVC)
    target_compile_options(logai_web_server PRIVATE /O2)
endif() 