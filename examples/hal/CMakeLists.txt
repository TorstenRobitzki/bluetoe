cmake_policy(SET CMP0079 NEW)

add_library(example_hal STATIC io.cpp)

if (CMAKE_CROSSCOMPILING)
    # to get the required include path
    target_link_libraries(example_hal PRIVATE toolchain::${BINDING})
endif()

target_include_directories(example_hal PUBLIC ..)