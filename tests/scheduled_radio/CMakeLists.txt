cmake_minimum_required(VERSION 3.13)

project(scheduled_radio_tester C CXX ASM)

if (CMAKE_CROSSCOMPILING)

    if (NOT DEFINED NRF5_SDK_ROOT)
        message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
    endif()

    add_compile_options(-ffunction-sections -fdata-sections)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ftemplate-backtrace-limit=0>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
    add_compile_options(-Wno-psabi)

    # Optimizations / Debug
    add_compile_options($<IF:$<CONFIG:Debug>,-O0,-Os>)
    add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

    # Half the RAM to be heap (128kByte)
    #add_compile_definitions(-D__HEAP_SIZE=16384)

    # Regardles of build type: Debug informations just make it into the elf file, never into the final binary
    add_compile_options(-g)
    add_compile_options(-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=soft)
    add_link_options(-g)
    add_link_options(LINKER:--gc-sections LINKER:--warn-common)
    add_link_options(-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=soft)

    # Standard libraries
    add_link_options(--sysroot=/usr/local/arm/arm-none-eabi -lstdc++ -lsupc++ --specs=nano.specs -static)

    add_compile_definitions(
        -DCATCH_CONFIG_NO_POSIX_SIGNALS=1
        -DCATCH_CONFIG_NOSTDOUT=1
        -DCATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1)

    add_subdirectory(/Users/todi/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)

    add_library(binding_dummy INTERFACE)
    add_library(toolchain::dummy ALIAS binding_dummy)
    set(BINDING dummy)
    add_subdirectory( ${CMAKE_CURRENT_LIST_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/bluetoe)

    add_executable(tester
        tester.cpp
        radio.cpp
        radio_callbacks.cpp
        serialize.cpp
        test_framework_support.cpp
        system_nrf52.c
        gcc_startup_nrf52840.S
        external_flash.cpp)

    set_property(TARGET tester PROPERTY CXX_STANDARD 20)
    set_target_properties(tester
        PROPERTIES
            OUTPUT_NAME tester.elf)

    target_link_libraries(tester PRIVATE Catch2::Catch2WithMain bluetoe::link_layer)
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

    function(artifacts_target target)
        add_custom_target(${target}.artifacts ALL
                COMMAND ${CMAKE_OBJCOPY} -S -O ihex ${target}.elf ${target}.hex
                COMMAND ${CMAKE_OBJDUMP} -hS ${target}.elf > ${target}.lss
                COMMAND ${CMAKE_SIZE} ${target}.elf
                )

        add_dependencies(${target}.artifacts ${target})

        if (DEFINED ${target}_JLINK)
            message("nrfjprog ${${target}_JLINK} selected")
            set(SELECT_JLINK_DEVICE --snr ${${target}_JLINK})
        endif()

        add_custom_target(${target}.flash
            COMMAND nrfjprog --chiperase --family NRF52 ${SELECT_JLINK_DEVICE} --program ${target}.hex --verify --reset --log
            VERBATIM
            )

        add_dependencies(${target}.flash ${target}.artifacts)
    endfunction()

    artifacts_target(tester)


else()

    find_package( Boost REQUIRED )

    add_executable(rpc_tests rpc_tests.cpp)
    target_include_directories(rpc_tests SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
    set_property(TARGET rpc_tests PROPERTY CXX_STANDARD 20)
endif()

add_subdirectory(nrf52)