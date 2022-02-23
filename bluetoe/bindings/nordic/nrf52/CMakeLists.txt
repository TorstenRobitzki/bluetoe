add_library(
    bluetoe_bindings_nrf52
    STATIC
    EXCLUDE_FROM_ALL
    nrf52.cpp
    security_tool_box.cpp
)

add_library(bluetoe::bindings::nrf52 ALIAS bluetoe_bindings_nrf52)

target_include_directories(bluetoe_bindings_nrf52
    PUBLIC
        include
)

target_link_libraries(bluetoe_bindings_nrf52
    PUBLIC
        bluetoe::utility
        bluetoe::sm
        bluetoe::iface
        bluetoe::link_layer
        bluetoe::bindings::nordic
    PRIVATE
        bluetoe::bindings::nrf::uecc
        toolchain::${BINDING}
)

target_compile_features(bluetoe_bindings_nrf52 INTERFACE cxx_std_11)
target_compile_options(bluetoe_bindings_nrf52 INTERFACE -Wall -pedantic -Wextra -Wfatal-errors -Wno-parentheses)
