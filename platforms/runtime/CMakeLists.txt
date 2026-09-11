# The C++ runtime that replaces newlib's: zero initialisation, constructors, main(). Compiled
# into every firmware, like assert.
add_library(runtime_gcc INTERFACE)
add_library(runtime::gcc ALIAS runtime_gcc)

target_sources(runtime_gcc
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/runtime_gcc.cpp)
