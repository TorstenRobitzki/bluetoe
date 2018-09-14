cmake_minimum_required(VERSION 3.10)
# Prevent in source build, add this options before project keyword
project(lib_bluetoe CXX)

add_subdirectory(bluetoe)

if (NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()
