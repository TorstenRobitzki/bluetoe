add_library(bluetoe_iface INTERFACE)
add_library(bluetoe::iface ALIAS bluetoe_iface)
target_include_directories(bluetoe_iface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bluetoe_iface INTERFACE bluetoe::linklayer bluetoe::sm bluetoe::services)
add_subdirectory(link_layer)
add_subdirectory(utility)
add_subdirectory(sm)
add_subdirectory(services)


