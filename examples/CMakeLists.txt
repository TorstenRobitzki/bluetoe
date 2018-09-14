cmake_minimum_required(VERSION 3.10)
# Prevent in source build, add this options before project keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(bluetoe_examples CXX)

if (CMAKE_CROSSCOMPILING)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0 -fvisibility-inlines-hidden -fno-rtti -fno-exceptions")
    add_subdirectory(${SOC}_toolchain_support)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/bluetoe)
    function(add_bluetoe_example target_name)
        add_executable(${target_name} ${target_name}.cpp runtime.cpp)
        add_dependencies(${target_name} linker_script)
        target_compile_options(${target_name} PRIVATE  )
        target_link_libraries(${target_name} PRIVATE bluetoe::iface
                                             PRIVATE bluetoe::bindings::${SOC}
                                             PRIVATE startup::${SOC}
                                             PRIVATE toolchain::${SOC}
                                -lm -lstdc++ -lsupc++
                                -T${CMAKE_CURRENT_BINARY_DIR}/${SOC}_toolchain_support/linker_script.ld
                                -Wl,--gc-sections -Wl,--warn-common
                                -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${target_name}.map -nostdlib
                                )
    endfunction()
    add_bluetoe_example(blinky)
else()
    message(error " examples can only be cross compiled to arm cortex-m, missing -Dtoolchain_file on cmake invocation ? ")
endif ()
