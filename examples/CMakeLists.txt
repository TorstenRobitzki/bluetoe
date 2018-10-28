cmake_minimum_required(VERSION 3.10)
# Prevent in source build, add this options before project keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(bluetoe_examples CXX C)

if (NOT CMAKE_CROSSCOMPILING)
    message(ERROR " examples can only be cross compiled to arm cortex-m, missing -Dtoolchain_file on cmake invocation?")
    return()
endif()
if (NOT BINDING)
    message(ERROR " A binding must be defined. Use -DBINDING=nrf51. Current available bindings are nrf51, nrf52")
    return()
endif()

# include hardware specific compile options and definitions that must be apllied to the whole project
include(${BINDING}_toolchain_support/platform.cmake)
#set global compile options that are hardware independent, these will be used to build the applications (examples)
#and will also be used to build bluetoe library
# add global compile options
add_compile_options(-ffunction-sections -fdata-sections)
#toolchain support targets, these are C and asm only, dont add cpp flags yet
add_subdirectory(${BINDING}_toolchain_support)
#add global options to cpp targets
add_compile_options(-ftemplate-backtrace-limit=0 -fvisibility-inlines-hidden -fno-rtti -fno-exceptions)
#bluetoe library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/bluetoe)
function(add_bluetoe_example target_name)
    add_executable(${target_name} ${target_name}.cpp runtime.cpp)
    set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${target_name}.elf)
    add_dependencies(${target_name} linker_script)
    target_compile_options(${target_name} PRIVATE  )
    target_link_libraries(${target_name} PRIVATE bluetoe::iface
                                         bluetoe::bindings::nrf51
                                         startup::${BINDING}
                                         toolchain::${BINDING}
                            -lm -lstdc++ -lsupc++
                            -T${CMAKE_CURRENT_BINARY_DIR}/${BINDING}_toolchain_support/linker_script.ld
                            -Wl,--gc-sections -Wl,--warn-common
                            -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${target_name}.map -nostdlib
                            )
    add_custom_target(${target_name}.artifacts ALL
            COMMAND ${CMAKE_OBJCOPY} -S -O ihex ${target_name}.elf ${target_name}.hex
            COMMAND ${CMAKE_OBJCOPY} -S -O binary --only-section=.text ${target_name}.elf ${target_name}.bin
            COMMAND ${CMAKE_OBJDUMP} -hS ${target_name}.elf > ${target_name}.lss
            COMMAND ${CMAKE_SIZE} ${target_name}.elf
            )
    add_dependencies(${target_name}.artifacts ${target_name})
    add_custom_target(${target_name}.flash
            COMMAND nrfjprog --chiperase --program ${target_name}.hex)
    add_dependencies(${target_name}.flash ${target_name}.artifacts)
endfunction()
add_bluetoe_example(blinky)
add_bluetoe_example(thermometer)
add_bluetoe_example(cycling_speed_and_cadence)
add_bluetoe_example(bootloader)

