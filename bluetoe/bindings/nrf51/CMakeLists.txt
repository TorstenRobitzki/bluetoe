add_library(bluetoe_bindings_nrf51 STATIC
            nrf51.cpp
            uECC.c)
add_library(bluetoe::bindings::nrf51 ALIAS bluetoe_bindings_nrf51)

target_include_directories(bluetoe_bindings_nrf51 PUBLIC include)

target_link_libraries(bluetoe_bindings_nrf51 PUBLIC bluetoe::utility bluetoe::sm bluetoe::iface bluetoe::link_layer PRIVATE toolchain::${BINDING})
target_compile_features(bluetoe_bindings_nrf51 PRIVATE cxx_std_11)
target_compile_options(bluetoe_bindings_nrf51 PRIVATE -Wall -pedantic -Wextra -Wfatal-errors -Wno-parentheses)
target_compile_definitions(bluetoe_bindings_nrf51 PRIVATE uECC_CURVE=uECC_secp256r1 uECC_ASM=1)
set_source_files_properties(uECC.c PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)
