cmake_minimum_required(VERSION 3.13)

# Prevent in source build, add this options before project keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(lib_bluetoe CXX)

option(BLUETOE_BUILD_UNIT_TESTS "If true, unit test targets are added are build.")

# Libray required by everything in Bluetoe. If there is need to add build options,
# add them on bluetoe::iface.
add_library(bluetoe_iface INTERFACE)
add_library(bluetoe::iface ALIAS bluetoe_iface)
target_compile_features(bluetoe_iface INTERFACE cxx_std_11)
target_include_directories(bluetoe_iface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Link Layer implementation based on radio hardware
add_subdirectory(bluetoe/link_layer)

# Link Layer implementation based on HCI over some serial interface
add_subdirectory(bluetoe/hci)

# Utilities
add_subdirectory(bluetoe/utility)

# Security Manager
add_subdirectory(bluetoe/sm)

# Predefined Services (like DIS, CSC and so on)
add_subdirectory(bluetoe/services)

if (CMAKE_CROSSCOMPILING)
    add_subdirectory(bluetoe/bindings)
endif()

if (NOT CMAKE_CROSSCOMPILING AND BLUETOE_BUILD_UNIT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
