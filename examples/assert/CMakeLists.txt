add_library(arm_assert INTERFACE)
add_library(assert::arm ALIAS arm_assert)

target_sources(arm_assert
    INTERFACE
        assert.cpp)

target_include_directories(arm_assert
    SYSTEM INTERFACE
        .)
