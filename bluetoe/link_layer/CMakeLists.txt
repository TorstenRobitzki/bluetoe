add_library(bluetoe_linklayer STATIC
            delta_time.cpp
            abs_time.cpp
            channel_map.cpp
            connection_details.cpp
            connection_parameters.cpp
            radio_properties.cpp)

add_library(bluetoe::link_layer ALIAS bluetoe_linklayer)

target_include_directories(bluetoe_linklayer PUBLIC include)
target_link_libraries(bluetoe_linklayer PUBLIC bluetoe::utility bluetoe::sm bluetoe::iface)
target_compile_features(bluetoe_linklayer PRIVATE cxx_std_20)
target_compile_options(bluetoe_linklayer PRIVATE -Wall -pedantic -Wextra -Wfatal-errors)
