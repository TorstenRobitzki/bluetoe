cmake_minimum_required(VERSION 3.13)

# the toolchain and the hardware, see platforms/README.md
include(${CMAKE_CURRENT_LIST_DIR}/../platforms/cmake/firmware_project.cmake)

project(bluetoe_examples CXX C)

include(${CMAKE_CURRENT_LIST_DIR}/../platforms/cmake/firmware.cmake)

# The CC2340 has the spike of its radio port and nothing else yet: the Radio Control Layer
# advertising on its own, watched by the tester.
if (BINDING STREQUAL cc23x0)
    add_bluetoe_firmware(cc2340_advertiser sdk::cc23x0 cc2340_advertiser.c)
    add_sysconfig(cc2340_advertiser ${CMAKE_CURRENT_SOURCE_DIR}/cc2340_advertiser.syscfg)

    return()
endif()

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

# blinky with and without encryption on each sleep clock of the radio: the firmware the power
# figures in the README are measured on. The plain examples run on the default, the sleep clock
# synthesized from the crystal; the variants name the other two through a definition.
function(add_blinky_variant target_name source sleep_clock)
    add_bluetoe_firmware(${target_name} bluetoe::bindings::${BINDING} ${source}.cpp)
    target_link_libraries(${target_name} PRIVATE spl)
    target_compile_definitions(${target_name} PRIVATE BLUETOE_EXAMPLE_SLEEP_CLOCK=${sleep_clock})
endfunction()

add_blinky_variant(blinky_lfxo blinky sleep_clock_crystal_oscillator)
add_blinky_variant(blinky_lfrc blinky calibrated_rc_sleep_clock)
add_blinky_variant(blinky_without_encryption_lfxo blinky_without_encryption sleep_clock_crystal_oscillator)
add_blinky_variant(blinky_without_encryption_lfrc blinky_without_encryption calibrated_rc_sleep_clock)

# The radio of bluetoe/scheduled_radio2.hpp does not announce the synchronized user timer
# yet; the example that needs it waits for that.
#
# add_bluetoe_example(synchronized_callbacks)
