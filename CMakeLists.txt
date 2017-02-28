cmake_minimum_required(VERSION 2.8.7)

enable_testing()

include_directories(${CMAKE_SOURCE_DIR})

if (DEFINED NDEBUG)
    add_definitions(-DNDEBUG=1)
endif()

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -pedantic -Wextra -Wfatal-errors -Wno-unused-parameter")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-logical-op-parentheses")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # using GCC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-parentheses")
endif()

add_subdirectory(bluetoe)
add_subdirectory(tests)
add_subdirectory(examples)
