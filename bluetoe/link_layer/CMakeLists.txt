add_library(bluetoe_linklayer STATIC
            delta_time.cpp
            address.cpp
            channel_map.cpp
            connection_details.cpp)
add_library(bluetoe::linklayer ALIAS bluetoe_linklayer)

target_include_directories(bluetoe_linklayer PUBLIC include)
target_compile_features(bluetoe_linklayer PRIVATE cxx_std_11)
target_compile_options(bluetoe_linklayer PRIVATE -Wall -pedantic -Wextra -Wfatal-errors)
