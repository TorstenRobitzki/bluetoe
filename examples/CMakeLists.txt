cmake_minimum_required(VERSION 3.13)

# Prevent in source build, add this options before project keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(AVAILABLE_BOARDS PCA10056 PCA10040)
set(AVAILABLE_BINDINGS NRF52840 NRF52833 NRF52832 NRF52820 NRF52811 NRF52810 NRF52805)

include(./cmake/deduce.cmake)

# Check BLUETOE_BOARD and BLUETOE_BINDING for correct values / combinations
if (DEFINED BLUETOE_BOARD)
    set_property(CACHE BLUETOE_BOARD PROPERTY STRINGS ${AVAILABLE_BOARDS})

    if (DEFINED BLUETOE_BINDING)
        message(FATAL_ERROR "'BLUETOE_BOARD' is set to '${BLUETOE_BOARD}' while 'BLUETOE_BINDING' is set to '${BLUETOE_BINDING}'. It is not expected to have both variables beeing set!")
    endif()

    if (NOT ${BLUETOE_BOARD} IN_LIST AVAILABLE_BOARDS)
        message(FATAL_ERROR "'BLUETOE_BOARD' is set to '${BLUETOE_BOARD}', which is an invalid value. Supported values are: ${AVAILABLE_BOARDS}")
    endif()

    deduce_binding(BLUETOE_BINDING ${BLUETOE_BOARD})
else()
    if (NOT DEFINED BLUETOE_BINDING)
        message(FATAL_ERROR "For a custom hardware, the 'BLUETOE_BINDING' cache variable is expected to be set.")
    endif()

    if (NOT ${BLUETOE_BINDING} IN_LIST AVAILABLE_BINDINGS)
        message(FATAL_ERROR "'BLUETOE_BINDING' is set to '${BLUETOE_BINDING}', which is an invalid value. Supported values are: ${AVAILABLE_BINDINGS}")
    endif()

    set_property(CACHE BLUETOE_BINDING PROPERTY STRINGS ${AVAILABLE_BINDINGS})
endif()


# Currently all examples are compiled with arm-none-eabi-gcc
set(CMAKE_TOOLCHAIN_FILE ./cmake/gcc-arm-none-eabi.cmake)

project(bluetoe_examples CXX C)

# set_preprocessore_macros(${BLUETOE_BOARD} ${BLUETOE_BINDING})
deduce_bluetoe_binding(BINDING ${BLUETOE_BINDING})

message("BLUETOE_BOARD: ${BLUETOE_BOARD}")
message("BLUETOE_BINDING: ${BLUETOE_BINDING}")
message("BINDING: ${BINDING}")

# include hardware specific compile options and definitions that must be apllied to the whole project
include(${BINDING}_toolchain_support/platform.cmake)

# set global compile options that are hardware independent, these will be used to build the applications (examples)
# and will also be used to build bluetoe library
# Put every function and object into it's own section and ask the linker to remove unreferenced sections
add_compile_options(-ffunction-sections -fdata-sections)
add_link_options(LINKER:--gc-sections LINKER:--warn-common)

# add global options to cpp targets
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ftemplate-backtrace-limit=0>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)

# Optimizations / Debug
add_compile_options($<IF:$<CONFIG:Debug>,-O0,-Os>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# Regardles of build type: Debug informations just make it into the elf file, never into the final binary
add_compile_options(-g)
add_link_options(-g)

# Standard libraries
add_compile_options(-nostdlib)
add_link_options(--sysroot=/usr/local/arm/arm-none-eabi -lm -lstdc++ -lsupc++ -nostdlib --specs=nano.specs -static)

# toolchain support targets
add_subdirectory(${BINDING}_toolchain_support)

# Resonale implementation of assert (no __PRETTY_FUNCTION__)
add_subdirectory(assert)

# bluetoe library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/bluetoe)

# Support for basic peripherals used in the examples
add_subdirectory(spl)

# Additional settings for the Bluetoe library itself:
target_link_libraries(bluetoe_iface INTERFACE assert::arm)

function(add_bluetoe_example target_name)
    add_executable(${target_name} ${target_name}.cpp runtime_gcc.cpp)
    set_target_properties(${target_name}
        PROPERTIES
            OUTPUT_NAME ${target_name}.elf)

    add_linker_script(${target_name})

    target_link_libraries(${target_name}
        PUBLIC
            assert::arm
        PRIVATE
            spl
            toolchain::${BINDING}
            startup::${BINDING}
            bluetoe::bindings::${BINDING}
    )

    add_custom_target(${target_name}.artifacts ALL
            COMMAND ${CMAKE_OBJCOPY} -S -O ihex ${target_name}.elf ${target_name}.hex
            COMMAND ${CMAKE_OBJCOPY} -S -O binary --only-section=.text ${target_name}.elf ${target_name}.bin
            COMMAND ${CMAKE_OBJDUMP} -hS ${target_name}.elf > ${target_name}.lss
            COMMAND ${CMAKE_SIZE} ${target_name}.elf
            )
    add_dependencies(${target_name}.artifacts ${target_name})


    define_flash_command(${target_name})
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
