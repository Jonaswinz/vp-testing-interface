cmake_minimum_required(VERSION 3.12)
project(test)

# Add the library (either from install or source)
add_subdirectory(../../ vp-build)

# Create the main executable
add_executable(test main.cpp)

# Link against the library
target_link_libraries(test PRIVATE vp-testing-interface)

# Include the headers
target_include_directories(test PRIVATE ../../include)