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

# The scheduled radio of bluetoe/scheduled_radio2.hpp, developed beside the binding above. A
# firmware links one of the two and never both, as both define the radio's interrupt handlers;
# security_tool_box.cpp is compiled into both for the same reason.
add_library(
    bluetoe_bindings_nrf52_radio
    STATIC
    EXCLUDE_FROM_ALL
    nrf52_radio.cpp
    security_tool_box.cpp
)

add_library(bluetoe::bindings::nrf52_radio ALIAS bluetoe_bindings_nrf52_radio)

target_include_directories(bluetoe_bindings_nrf52_radio
    PUBLIC
        include
)

target_link_libraries(bluetoe_bindings_nrf52_radio
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

set_target_properties(bluetoe_bindings_nrf52_radio PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_options(bluetoe_bindings_nrf52_radio INTERFACE -Wall -pedantic -Wextra -Wfatal-errors -Wno-parentheses)
