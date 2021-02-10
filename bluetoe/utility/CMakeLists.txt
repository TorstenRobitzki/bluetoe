add_library(bluetoe_utility STATIC
            address.cpp)
add_library(bluetoe::utility ALIAS bluetoe_utility)

target_include_directories(bluetoe_utility PUBLIC include)
target_compile_features(bluetoe_utility PRIVATE cxx_std_11)
target_compile_options(bluetoe_utility PRIVATE -Wall -pedantic -Wextra -Wfatal-errors)
