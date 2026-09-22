# The nRF52 rigs: one firmware per rig file, around the platform's radio, over the shared UART.
function(add_nrf52_dut_rig rig)
    add_bluetoe_firmware(${rig} bluetoe::bindings::${BINDING} ${rig}.cpp)
    target_link_libraries(${rig} PRIVATE test_rig::nrf52_uart)
    target_compile_definitions(${rig} PRIVATE DUT_BUILD_IDENTIFIER="${DUT_BUILD_IDENTIFIER}")
endfunction()

add_nrf52_dut_rig(nrf52_dut)
