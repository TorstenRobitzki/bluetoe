cmake_minimum_required(VERSION 3.13)

# Prevent in source build, add this options before project keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

project(bluetoe_examples CXX C)

if (NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR " examples can only be cross compiled to arm cortex-m, missing -Dtoolchain_file on cmake invocation?")
endif()

if (NOT BINDING)
    message(FATAL_ERROR " A binding must be defined. Use -DBINDING=nrf51. Current available bindings are nrf51, nrf52")
endif()

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
include_directories(SYSTEM ./assert)

# bluetoe library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/bluetoe)

function(add_bluetoe_example target_name)
    add_executable(${target_name} ${target_name}.cpp runtime.cpp)
    set_target_properties(${target_name}
        PROPERTIES
            OUTPUT_NAME ${target_name}.elf)

    add_linker_script(${target_name})

    target_link_libraries(${target_name} PRIVATE bluetoe::iface
                                         bluetoe::bindings::nrf51
                                         startup::${BINDING}
                                         toolchain::${BINDING}
                                         assert::arm
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
add_bluetoe_example(blinky_without_encryption)
add_bluetoe_example(thermometer)
add_bluetoe_example(cycling_speed_and_cadence)
add_bluetoe_example(bootloader)
add_bluetoe_example(scheduled_radio_tests)

