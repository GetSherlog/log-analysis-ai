# Source files (all files in the current directory, excluding test and benchmark files)
file(GLOB SOURCES "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "perf_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "large_perf_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "preprocessor_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "simd_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "neon_simd_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "feature_extractor_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "logbert_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "logbert_unit_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "dbscan_test\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "dbscan_kdtree_test\\.cpp$")
file(GLOB HEADERS "*.h")

# Create library
add_library(logai ${SOURCES} ${HEADERS})
target_include_directories(logai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Find abseil-cpp package
find_package(absl REQUIRED)

# Link with dependencies
target_link_libraries(logai 
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add performance test executable
add_executable(perf_test perf_test.cpp)
target_link_libraries(perf_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add large file performance test executable
add_executable(large_perf_test large_perf_test.cpp)
target_link_libraries(large_perf_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add preprocessor test executable
add_executable(preprocessor_test preprocessor_test.cpp)
target_link_libraries(preprocessor_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add feature extractor test executable
add_executable(feature_extractor_test feature_extractor_test.cpp)
target_link_libraries(feature_extractor_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add SIMD optimization test executable
add_executable(simd_test simd_test.cpp)
target_link_libraries(simd_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add NEON SIMD optimization test executable
add_executable(neon_simd_test neon_simd_test.cpp)
target_link_libraries(neon_simd_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add DBSCAN test executable
add_executable(dbscan_test dbscan_test.cpp)
target_link_libraries(dbscan_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add KD-tree optimized DBSCAN test executable
add_executable(dbscan_kdtree_test dbscan_kdtree_test.cpp)
target_link_libraries(dbscan_kdtree_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add LogBERT test executable
add_executable(logbert_test logbert_test.cpp)
target_link_libraries(logbert_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Add LogBERT unit test executable
add_executable(logbert_unit_test logbert_unit_test.cpp)
target_link_libraries(logbert_unit_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
    PRIVATE absl::strings
)

# Enable SIMD optimizations
# Add compiler-specific flags for SIMD
if(NOT DEFINED DISABLE_SIMD OR NOT DISABLE_SIMD)
    message(STATUS "SIMD optimizations ENABLED")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCXXCompilerFlag)
        
        # Check if we're on ARM architecture
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
            # Check for ARM NEON support
            check_cxx_compiler_flag("-march=armv8-a+simd" COMPILER_SUPPORTS_NEON)
            if(COMPILER_SUPPORTS_NEON)
                message(STATUS "ARM NEON SIMD instructions enabled")
                target_compile_options(logai PRIVATE -march=armv8-a+simd)
                target_compile_options(simd_test PRIVATE -march=armv8-a+simd)
                target_compile_options(neon_simd_test PRIVATE -march=armv8-a+simd)
                target_compile_definitions(logai PRIVATE USE_NEON_SIMD=1)
                target_compile_definitions(simd_test PRIVATE USE_NEON_SIMD=1)
                target_compile_definitions(neon_simd_test PRIVATE USE_NEON_SIMD=1)
            endif() # Fixed trailing endif
        else()
            check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
            if(COMPILER_SUPPORTS_AVX2)
                target_compile_options(logai PRIVATE -mavx2)
                target_compile_options(simd_test PRIVATE -mavx2)
                target_compile_options(neon_simd_test PRIVATE -mavx2)
            endif() # Fixed trailing endif
            # Check for SSE4.2 support
            check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
            if(COMPILER_SUPPORTS_SSE42)
                target_compile_options(logai PRIVATE -msse4.2)
                target_compile_options(simd_test PRIVATE -msse4.2)
                target_compile_options(neon_simd_test PRIVATE -msse4.2)
            endif() # Fixed trailing endif
        endif() # Fixed trailing endif
    else()
        # For MSVC, we'll enable AVX2 if available
        if(MSVC)
            target_compile_options(logai PRIVATE /arch:AVX2)
            target_compile_options(simd_test PRIVATE /arch:AVX2)
            target_compile_options(neon_simd_test PRIVATE /arch:AVX2)
        endif() # Fixed trailing endif
    endif() # Fixed trailing endif
else()
    message(STATUS "SIMD optimizations DISABLED")
endif() # Fixed trailing endif

# Install
install(TARGETS logai perf_test large_perf_test preprocessor_test simd_test neon_simd_test feature_extractor_test logbert_test logbert_unit_test dbscan_test dbscan_kdtree_test
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(FILES ${HEADERS} DESTINATION include/logai) 