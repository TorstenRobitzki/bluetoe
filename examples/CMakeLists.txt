cmake_minimum_required(VERSION 3.13)

# the toolchain and the hardware, see platforms/README.md
include(${CMAKE_CURRENT_LIST_DIR}/../platforms/cmake/firmware_project.cmake)

project(bluetoe_examples CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/../platforms/cmake/firmware.cmake)

# Support for basic peripherals used in the examples
add_subdirectory(spl)

function(add_bluetoe_example target_name)
    add_bluetoe_firmware(${target_name} ${target_name}.cpp)
    target_link_libraries(${target_name} PRIVATE spl)
endfunction()

add_bluetoe_example(blinky)
add_bluetoe_example(blinky_with_oob)
add_bluetoe_example(blinky_without_encryption)
add_bluetoe_example(blinky_with_lesc_and_legacy_pairing)
add_bluetoe_example(thermometer)
add_bluetoe_example(cycling_speed_and_cadence)
add_bluetoe_example(bootloader)
# TODO: scheduled radio tests is not an actual example, and needs to be
# TODO: generalized to work with all scheduled radio implementations (nrf51, nrf52, ... )
# TODO: add_bluetoe_example(scheduled_radio_tests)
add_bluetoe_example(keyboard)
add_bluetoe_example(nrf52_high_cpu_load)
add_bluetoe_example(custom_advertising)
add_bluetoe_example(gpio)
add_bluetoe_example(synchronized_callbacks)
