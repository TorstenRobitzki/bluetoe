cmake_minimum_required(VERSION 3.13)

# the toolchain and the hardware, see platforms/README.md
include(${CMAKE_CURRENT_LIST_DIR}/../platforms/cmake/firmware_project.cmake)

project(bluetoe_examples CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/../platforms/cmake/firmware.cmake)

# Support for basic peripherals used in the examples
add_subdirectory(spl)

function(add_bluetoe_example target_name)
    add_bluetoe_firmware(${target_name} bluetoe::bindings::${BINDING} ${target_name}.cpp)
    target_link_libraries(${target_name} PRIVATE spl)
endfunction()

add_bluetoe_example(blinky)
add_bluetoe_example(blinky_with_oob)
add_bluetoe_example(blinky_without_encryption)
add_bluetoe_example(blinky_with_lesc_and_legacy_pairing)
add_bluetoe_example(thermometer)
add_bluetoe_example(cycling_speed_and_cadence)
add_bluetoe_example(bootloader)
add_bluetoe_example(keyboard)
add_bluetoe_example(custom_advertising)
add_bluetoe_example(gpio)

add_bluetoe_example(nrf52_high_cpu_load)

# The radio of bluetoe/scheduled_radio2.hpp does not announce the synchronized user timer
# yet; the example that needs it waits for that.
#
# add_bluetoe_example(synchronized_callbacks)
