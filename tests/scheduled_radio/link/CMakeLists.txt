add_library(scheduled_radio_link INTERFACE)

add_library(scheduled_radio::link ALIAS scheduled_radio_link)
target_include_directories(scheduled_radio_link INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(scheduled_radio_link INTERFACE bluetoe::link_layer)
