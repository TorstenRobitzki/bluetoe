cmake_minimum_required(VERSION 3.13)

project(scheduled_radio_tester C CXX ASM)

if (NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "This file is not expected to be included in a not cross compile scenario!")
endif()

find_package( Boost REQUIRED )

if (NOT DEFINED NRF5_SDK_ROOT)
    message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
endif()

add_compile_options(-ffunction-sections -fdata-sections)
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
add_link_options(LINKER:--gc-sections LINKER:--warn-common)

# Standard libraries
# add_compile_options(-nostdlib)
# add_link_options(--sysroot=/usr/local/arm/arm-none-eabi -lstdc++ -lsupc++ -nostdlib --specs=nano.specs -static)

add_compile_definitions(
    -DCATCH_CONFIG_NO_POSIX_SIGNALS=1
    -DCATCH_CONFIG_NOSTDOUT=1
    -DCATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

add_subdirectory(/Users/todi/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)

add_executable(tester
    tester.cpp
    test_framework_support.cpp
    system_nrf52.c
    gcc_startup_nrf52840.S)

target_link_libraries(tester PRIVATE Catch2::Catch2WithMain)
target_include_directories(tester
    SYSTEM PRIVATE
        ${Boost_INCLUDE_DIR}
        ${NRF5_SDK_ROOT}/modules/nrfx/mdk/
        ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include/ )

target_compile_definitions(tester
    PRIVATE
        -DNRF52840_XXAA=1)

target_link_directories(tester PRIVATE ${CMAKE_CURRENT_LIST_DIR})
set_property(
    TARGET tester
    PROPERTY
        LINK_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/nrf52840_xxaa.ld
            ${CMAKE_CURRENT_SOURCE_DIR}/nrf_common.ld)

target_link_options(tester PRIVATE -T nrf52840_xxaa.ld -Wl,-Map=tester.map)

#target_compile_definitions(tester PRIVATE -DCLOCK_MONOTONIC=1)