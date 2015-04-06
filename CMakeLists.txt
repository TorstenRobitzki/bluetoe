cmake_minimum_required(VERSION 2.8.12)

include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -pedantic")

add_subdirectory(tests)
add_subdirectory(examples)