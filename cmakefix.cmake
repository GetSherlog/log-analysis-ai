# CMake module to fix target conflicts
# This helps resolve conflicts between Drogon's targets and our own targets

# For UUID library
if(TARGET UUID_lib)
  message(STATUS "UUID_lib target already exists, skipping creation")
else()
  find_package(UUID REQUIRED)
endif()

# For MySQL library
if(TARGET MySQL_lib)
  message(STATUS "MySQL_lib target already exists, skipping creation")
else()
  find_package(MySQL REQUIRED)
endif() 