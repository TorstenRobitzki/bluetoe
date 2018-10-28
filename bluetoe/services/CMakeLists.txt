add_library(bluetoe_services INTERFACE)
add_library(bluetoe::services ALIAS bluetoe_services)
target_include_directories(bluetoe_services INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
