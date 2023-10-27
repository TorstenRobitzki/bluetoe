if (CMAKE_CROSSCOMPILING)
    if (NOT DEFINED NRF5_SDK_ROOT)
        message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
    endif()

    add_executable(nrf52_scheduled_radio_dut EXCLUDE_FROM_ALL
        ../dut.cpp)

    target_compile_options(nrf52_scheduled_radio_dut PUBLIC -nostdlib)
    target_link_options(nrf52_scheduled_radio_dut PRIVATE -nostdlib)
    target_link_libraries(nrf52_scheduled_radio_dut PRIVATE bluetoe::link_layer)
    target_include_directories(nrf52_scheduled_radio_dut
        SYSTEM PRIVATE
            ${NRF5_SDK_ROOT}/modules/nrfx/mdk/
            ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include/ )

    set_property(TARGET nrf52_scheduled_radio_dut PROPERTY CXX_STANDARD 20)
    set_target_properties(nrf52_scheduled_radio_dut
        PROPERTIES
            OUTPUT_NAME nrf52_scheduled_radio_dut.elf)

    artifacts_target(nrf52_scheduled_radio_dut)
endif()