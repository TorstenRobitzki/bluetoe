cmake_minimum_required(VERSION 3.10)
# Prevent in source build, add this options before project keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(bluetoe_examples CXX C)

if (CMAKE_CROSSCOMPILING)
    #set global compile options that are hardware independent, these will be used to build the applications (examples)
    #and will also be used to build bluetoe library
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0 -fvisibility-inlines-hidden -fno-rtti -fno-exceptions")
    # set global compiler options that are hardware dependent
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m0 -mthumb -mabi=aapcs" )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m0 -mthumb -mabi=aapcs" )
    #toolchain support targets
    add_subdirectory(${SOC}_toolchain_support)
    #bluetoe library
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/bluetoe)
    function(add_bluetoe_example target_name)
        add_executable(${target_name} ${target_name}.cpp runtime.cpp)
        set_target_properties(${target_name} PROPERTIES OUTPUT_NAME ${target_name}.elf)
        add_dependencies(${target_name} linker_script)
        target_compile_options(${target_name} PRIVATE  )
        target_link_libraries(${target_name} PRIVATE bluetoe::iface
                                             bluetoe::bindings::${SOC}
                                             startup::${SOC}
                                             toolchain::${SOC}
                                -lm -lstdc++ -lsupc++
                                -T${CMAKE_CURRENT_BINARY_DIR}/${SOC}_toolchain_support/linker_script.ld
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
else()
    message(error " examples can only be cross compiled to arm cortex-m, missing -Dtoolchain_file on cmake invocation ? ")
endif ()

