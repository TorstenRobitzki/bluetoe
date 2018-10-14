add_library(bluetoe_utility INTERFACE)
add_library(bluetoe::utility ALIAS bluetoe_utility)

target_include_directories(bluetoe_utility INTERFACE include)
