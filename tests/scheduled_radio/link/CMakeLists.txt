add_library(scheduled_radio_link STATIC
    crc16.cpp
    status.cpp
)

add_library(scheduled_radio::link ALIAS scheduled_radio_link)
target_include_directories(scheduled_radio_link PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(scheduled_radio_link PUBLIC bluetoe::link_layer)
