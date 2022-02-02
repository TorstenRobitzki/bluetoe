add_library(bluetoe_bindings_nrf52 INTERFACE EXCLUDE_FROM_ALL)
add_library(bluetoe::bindings::nrf52 ALIAS bluetoe_bindings_nrf52)

target_include_directories(bluetoe_bindings_nrf52 INTERFACE include)

target_link_libraries(bluetoe_bindings_nrf52
    INTERFACE
        bluetoe::utility
        bluetoe::sm
        bluetoe::iface
        bluetoe::link_layer
        bluetoe::bindings::nrf::uecc
        bluetoe::bindings::nordic)

target_compile_features(bluetoe_bindings_nrf52 INTERFACE cxx_std_11)
target_compile_options(bluetoe_bindings_nrf52 INTERFACE -Wall -pedantic -Wextra -Wfatal-errors -Wno-parentheses)
