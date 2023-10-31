if (CMAKE_CROSSCOMPILING)
    add_subdirectory(../../../examples/nrf52_toolchain_support ${CMAKE_BINARY_DIR}/nrf52_toolchain_support)
    if (NOT DEFINED NRF5_SDK_ROOT)
        message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
    endif()

    add_executable(nrf52_scheduled_radio_dut EXCLUDE_FROM_ALL
        ../dut.cpp
        ../serialize.cpp
        gcc_startup_nrf52840.S
        system_nrf52.c
        runtime_gcc.cpp)

    target_compile_options(nrf52_scheduled_radio_dut PUBLIC -nostdlib)
    target_link_options(nrf52_scheduled_radio_dut PRIVATE -nostdlib)
    target_link_libraries(nrf52_scheduled_radio_dut PRIVATE startup::nrf52 bluetoe::bindings::nrf52_2)
    target_include_directories(nrf52_scheduled_radio_dut
        SYSTEM PRIVATE
            ${NRF5_SDK_ROOT}/modules/nrfx/mdk/
            ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include/ )

    set_property(TARGET nrf52_scheduled_radio_dut PROPERTY CXX_STANDARD 20)
    set_target_properties(nrf52_scheduled_radio_dut
        PROPERTIES
            OUTPUT_NAME nrf52_scheduled_radio_dut.elf)

    target_compile_definitions(nrf52_scheduled_radio_dut
        PRIVATE
            -DNRF52840_XXAA=1)

    target_link_directories(nrf52_scheduled_radio_dut PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    set_property(
        TARGET nrf52_scheduled_radio_dut
        PROPERTY
            LINK_DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/nrf52840_xxaa.ld
                ${CMAKE_CURRENT_SOURCE_DIR}/nrf_common.ld)

    target_link_options(nrf52_scheduled_radio_dut PRIVATE -T nrf52840_xxaa.ld -Wl,-Map=nrf52_scheduled_radio_dut.map)

    artifacts_target(nrf52_scheduled_radio_dut)
endif()