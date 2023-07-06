add_library(spl STATIC)

target_include_directories(spl PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(spl PUBLIC toolchain::nrf52 bluetoe_utility)

target_sources(spl PRIVATE temperature.cpp)