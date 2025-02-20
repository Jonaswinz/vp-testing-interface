cmake_minimum_required(VERSION 3.12)

project(vp-testing-interface)

set(src "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(inc "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(sources
    ${src}/testing_receiver.cpp
    ${src}/testing_communication.cpp
    ${src}/mq_testing_communication.cpp
    ${src}/pipe_testing_communication.cpp
    ${src}/mq_testing_client.cpp
)

# Create the library (Choose STATIC or SHARED)
add_library(vp-testing-interface STATIC ${sources}) # Change STATIC to SHARED for shared library

# Include the public headers
target_include_directories(vp-testing-interface PUBLIC ${inc})

# Set C++ standard
set_target_properties(vp-testing-interface PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

target_link_libraries(vp-testing-interface PUBLIC pthread rt)

# Install rules (optional, for system-wide usage)
install(TARGETS vp-testing-interface DESTINATION lib)
install(DIRECTORY ${inc}/ DESTINATION include) # Copy headers