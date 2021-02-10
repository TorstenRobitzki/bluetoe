add_library(bluetoe_hci INTERFACE)

add_library(bluetoe::hci ALIAS bluetoe_hci)

target_include_directories(bluetoe_hci INTERFACE include)
# target_compile_features(bluetoe_hci PRIVATE cxx_std_11)
# target_compile_options(bluetoe_hci PRIVATE -Wall -pedantic -Wextra -Wfatal-errors)
