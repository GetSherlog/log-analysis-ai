# LogAI Web Server CMakeLists.txt

# Set minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Source files
file(GLOB_RECURSE SERVER_SOURCES "*.cpp")
file(GLOB_RECURSE SERVER_HEADERS "*.h")

# Find Drogon package
find_package(Drogon REQUIRED)

# Create the web server executable
add_executable(logai_server ${SERVER_SOURCES} ${SERVER_HEADERS})

# Include directories
target_include_directories(logai_server 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${DROGON_INCLUDE_DIRS}
)

# Link the libraries
target_link_libraries(logai_server 
    PRIVATE logai
    PRIVATE ${DROGON_LIBRARIES}
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
    PRIVATE ${EIGEN3_LIBRARIES}
)

# Installation
install(TARGETS logai_server 
    RUNTIME DESTINATION bin
)

# Configure server properties
set_target_properties(logai_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(logai_server PRIVATE -O3 -march=native)
elseif(MSVC)
    target_compile_options(logai_server PRIVATE /O2)
endif() 