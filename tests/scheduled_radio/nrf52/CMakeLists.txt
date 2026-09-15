# The serial port of the instruments on the nRF52, the same code on a device under test and on
# the tester.
add_library(test_rig_nrf52_uart STATIC uart.cpp)
add_library(test_rig::nrf52_uart ALIAS test_rig_nrf52_uart)
target_link_libraries(test_rig_nrf52_uart PUBLIC scheduled_radio::link toolchain::nrf52)
set_target_properties(test_rig_nrf52_uart PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
