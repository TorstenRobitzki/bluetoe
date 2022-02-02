add_library(bluetoe_bindings_uECC STATIC
            uECC.c)

add_library(bluetoe::bindings::nrf::uecc ALIAS bluetoe_bindings_uECC)

target_include_directories(bluetoe_bindings_uECC PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(bluetoe_bindings_uECC PRIVATE uECC_CURVE=uECC_secp256r1 uECC_ASM=1)
set_source_files_properties(uECC.c PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)

