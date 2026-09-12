cmake_minimum_required(VERSION 3.13)

# The firmwares of the devices under test, one per platform and radio configuration
# (documentation/scheduled_radio_test_rig.md, decision 22). A firmware project on platforms/,
# like examples/; see README.md for building and flashing.
include(${CMAKE_CURRENT_LIST_DIR}/../../../platforms/cmake/firmware_project.cmake)

project(bluetoe_dut_rigs CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/../../../platforms/cmake/firmware.cmake)

# The build identifier a rig reports to the host: the source state it was built from. Taken at
# configure time, so it is stale between a commit and the next configure.
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE DUT_BUILD_IDENTIFIER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

if(NOT DUT_BUILD_IDENTIFIER)
    set(DUT_BUILD_IDENTIFIER "unidentified build")
endif()

message("DUT_BUILD_IDENTIFIER: ${DUT_BUILD_IDENTIFIER}")

# the code the rigs share with the host: frames, serialisation, the protocol
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../link ${CMAKE_BINARY_DIR}/link)

add_subdirectory(${BINDING})
