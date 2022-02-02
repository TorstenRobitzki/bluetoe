add_library(bluetoe_bindings_nrf51 STATIC EXCLUDE_FROM_ALL
            nrf51.cpp)
add_library(bluetoe::bindings::nrf51 ALIAS bluetoe_bindings_nrf51)

target_include_directories(bluetoe_bindings_nrf51 PUBLIC include)

target_link_libraries(bluetoe_bindings_nrf51
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

target_compile_features(bluetoe_bindings_nrf51 PRIVATE cxx_std_11)
target_compile_options(bluetoe_bindings_nrf51 PRIVATE -Wall -pedantic -Wextra -Wfatal-errors -Wno-parentheses)
