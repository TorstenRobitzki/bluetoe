if (NOT DEFINED ENV{BUILD_ON_TRAVIS})
    add_subdirectory(bindings)

    add_library(bluetoe link_layer/delta_time.cpp)
endif()