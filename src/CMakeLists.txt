# Source files (all files in the current directory, excluding perf_test.cpp)
file(GLOB SOURCES "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "perf_test\\.cpp$")
file(GLOB HEADERS "*.h")

# Create library
add_library(logai ${SOURCES} ${HEADERS})
target_include_directories(logai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link with dependencies
target_link_libraries(logai 
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
)

# Add performance test executable
add_executable(perf_test perf_test.cpp)
target_link_libraries(perf_test 
    PRIVATE logai
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE PkgConfig::ARROW
    PRIVATE PkgConfig::PARQUET
)

# Install
install(TARGETS logai perf_test
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(FILES ${HEADERS} DESTINATION include/logai) 