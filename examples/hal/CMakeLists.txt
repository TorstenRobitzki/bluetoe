cmake_policy(SET CMP0079 NEW)


if (CMAKE_CROSSCOMPILING)
    add_library(example_hal STATIC ${BINDING}_io.cpp)

    # to get the required include path
    target_link_libraries(example_hal PRIVATE toolchain::${BINDING})
else()
    add_library(example_hal STATIC io.cpp)
endif()

target_include_directories(example_hal PUBLIC ..)