cmake_minimum_required(VERSION 3.12)
project(client)

add_subdirectory(../../../ vp-build)

add_executable(client main.cpp)

target_link_libraries(client PRIVATE rt vp-testing-interface)

target_include_directories(client PRIVATE ../../../include)