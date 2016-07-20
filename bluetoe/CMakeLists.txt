if (NOT DEFINED ENV{BUILD_ON_TRAVIS})
    add_subdirectory(bindings)
endif()

add_library(bluetoe
    link_layer/delta_time.cpp
    link_layer/address.cpp
    link_layer/address_io.cpp
    link_layer/channel_map.cpp
    link_layer/connection_details.cpp)
