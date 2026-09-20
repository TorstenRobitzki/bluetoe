cmake_minimum_required(VERSION 3.13)

# The tester: one firmware for one board (documentation/scheduled_radio_test_rig.md, decision 23).
# A firmware project on platforms/ for the toolchain, the startup code and the flash target; it
# shares the link code and the UART with the devices under test and nothing else. See README.md
# for wiring, building and flashing.
set(BLUETOE_BOARD PCA10056 CACHE STRING "the development kit the tester runs on")

include(${CMAKE_CURRENT_LIST_DIR}/../../../platforms/cmake/firmware_project.cmake)

project(bluetoe_tester CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/../../../platforms/cmake/firmware.cmake)

# A pointer to an inline member function is hidden under -fvisibility-inlines-hidden, and so is
# a template over such pointers, the tester's function list; GCC then warns that the tester has
# greater visibility than its dispatcher. Visibility means nothing in a static firmware, so
# everything is hidden alike.
add_compile_options(-fvisibility=hidden)

# The build identifier the tester reports to the host: the source state it was built from, as
# for a device under test (decision 22).
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE TESTER_BUILD_IDENTIFIER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

if(NOT TESTER_BUILD_IDENTIFIER)
    set(TESTER_BUILD_IDENTIFIER "unidentified build")
endif()

message("TESTER_BUILD_IDENTIFIER: ${TESTER_BUILD_IDENTIFIER}")

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../link ${CMAKE_BINARY_DIR}/link)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../nrf52 ${CMAKE_BINARY_DIR}/port)

# The tester is an instrument on one nRF52840-DK and never runs on a device under test, so it
# uses the RAM of that board; the rig stays within the memory of the smallest nRF52.
set(BLUETOE_RAM_SIZE 256K)

add_bluetoe_firmware(tester test_rig::nrf52_uart tester.cpp platform.cpp)
target_compile_definitions(tester PRIVATE TESTER_BUILD_IDENTIFIER="${TESTER_BUILD_IDENTIFIER}")
set_target_properties(tester PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
