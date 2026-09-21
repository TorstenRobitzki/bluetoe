find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_library(scheduled_radio_host STATIC
    central.cpp
    proxy.cpp
    serial_transport.cpp
)

add_library(scheduled_radio::host ALIAS scheduled_radio_host)
target_include_directories(scheduled_radio_host SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(scheduled_radio_host PUBLIC scheduled_radio::link Threads::Threads)
