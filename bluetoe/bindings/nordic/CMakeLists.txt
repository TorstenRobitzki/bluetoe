add_library(bluetoe_bindings_nordic INTERFACE EXCLUDE_FROM_ALL)
add_library(bluetoe::bindings::nordic ALIAS bluetoe_bindings_nordic)

target_include_directories(bluetoe_bindings_nordic INTERFACE include)

add_subdirectory(uECC)
add_subdirectory(nrf51)
add_subdirectory(nrf52)

