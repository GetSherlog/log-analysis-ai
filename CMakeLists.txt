cmake_minimum_required(VERSION 3.10)
project(logai-cpp VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set compiler flags for optimization
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2 /arch:AVX2")
endif()

# Find dependencies
# Find Abseil
find_package(absl CONFIG REQUIRED)

# Find Eigen
find_package(Eigen3 3.4 REQUIRED NO_MODULE)
message(STATUS "Found Eigen3: ${EIGEN3_VERSION}")

# JSON library
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    # If not found, use FetchContent as fallback
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# Find Arrow and Parquet using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARROW REQUIRED IMPORTED_TARGET arrow)
pkg_check_modules(PARQUET REQUIRED IMPORTED_TARGET parquet)
message(STATUS "Found Arrow: ${ARROW_VERSION}")
message(STATUS "Found Parquet: ${PARQUET_VERSION}")

# Find Drogon package for the web server
find_package(Drogon QUIET)
if(NOT Drogon_FOUND)
    message(STATUS "Drogon not found. Will attempt to fetch and build it.")
    include(FetchContent)
    FetchContent_Declare(
        drogon
        GIT_REPOSITORY https://github.com/drogonframework/drogon.git
        GIT_TAG v1.8.6
    )
    FetchContent_MakeAvailable(drogon)
endif()

# Add src subdirectory
add_subdirectory(src)

# Add web_server subdirectory if Drogon is available
if(Drogon_FOUND OR TARGET Drogon::Drogon)
    add_subdirectory(src/web_server)
else()
    message(WARNING "Drogon not available. Web server will not be built.")
endif()

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/logai-cpp-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/logai-cpp-config-version.cmake"
    DESTINATION lib/cmake/logai-cpp
)

# Enable testing
enable_testing()

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration summary:")
message(STATUS "  C++ compiler:      ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ compiler ID:   ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  C++ compiler flags:${CMAKE_CXX_FLAGS}")
message(STATUS "  Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Arrow version:     ${ARROW_VERSION}")
message(STATUS "  Parquet version:   ${PARQUET_VERSION}")
if(Drogon_FOUND OR TARGET Drogon::Drogon)
    message(STATUS "  Drogon version:    ${Drogon_VERSION}")
    message(STATUS "  Web server:        Enabled")
else()
    message(STATUS "  Web server:        Disabled")
endif()
message(STATUS "") 