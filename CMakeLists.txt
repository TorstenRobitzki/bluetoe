cmake_minimum_required(VERSION 2.8.7)

include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -pedantic -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-logical-op-parentheses")

add_subdirectory(bluetoe)
add_subdirectory(tests)
add_subdirectory(examples)
