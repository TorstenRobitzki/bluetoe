add_library(spl INTERFACE)

target_include_directories(spl INTERFACE ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(spl INTERFACE bluetoe_utility)