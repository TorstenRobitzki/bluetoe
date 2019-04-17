cmake_policy(SET CMP0079 NEW)

add_library(example_hal STATIC io.cpp)
target_link_libraries(example_hal PRIVATE toolchain::${BINDING})
target_include_directories(example_hal PUBLIC ..)