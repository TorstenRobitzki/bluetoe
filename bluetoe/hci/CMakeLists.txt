cmake_policy(SET CMP0079 NEW)

add_library(bluetoe_hci INTERFACE)
add_library(bluetoe::hci ALIAS bluetoe_hci)

target_include_directories(bluetoe_hci INTERFACE include)
target_link_libraries(bluetoe_hci INTERFACE bluetoe::sm bluetoe::iface)
