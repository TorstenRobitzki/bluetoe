if (NOT DEFINED ENV{BUILD_ON_TRAVIS})
    add_subdirectory(bindings)
endif()