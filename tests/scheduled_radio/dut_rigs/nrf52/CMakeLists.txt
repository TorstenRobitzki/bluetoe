# The nRF52 rigs: the serial port they share, and one firmware per rig file.
add_library(dut_rig_nrf52_port STATIC uart.cpp)
add_library(dut_rig::nrf52_port ALIAS dut_rig_nrf52_port)
target_link_libraries(dut_rig_nrf52_port PUBLIC scheduled_radio::link toolchain::nrf52)
set_target_properties(dut_rig_nrf52_port PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

function(add_nrf52_dut_rig rig)
    add_bluetoe_firmware(${rig} bluetoe::bindings::${BINDING}_radio ${rig}.cpp)
    target_link_libraries(${rig} PRIVATE dut_rig::nrf52_port)
    target_compile_definitions(${rig} PRIVATE DUT_BUILD_IDENTIFIER="${DUT_BUILD_IDENTIFIER}")
    set_target_properties(${rig} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endfunction()

add_nrf52_dut_rig(nrf52_dut)
