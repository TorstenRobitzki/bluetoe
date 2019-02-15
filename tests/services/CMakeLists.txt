add_library(test_gatt test_gatt.cpp)
target_include_directories(test_gatt PUBLIC ../test_tools)
target_link_libraries(test_gatt PUBLIC bluetoe::iface bluetoe::utility)

add_and_register_test(cscs_tests)
add_and_register_test(bootloader_tests)
add_and_register_test(battery_tests)

target_link_libraries(bootloader_tests PRIVATE bluetoe::services test_gatt)
target_link_libraries(cscs_tests PRIVATE bluetoe::services test_gatt)
target_link_libraries(battery_tests PRIVATE bluetoe::services test_gatt)
