find_package(Boost REQUIRED)

# What a radio test is written with: the links to the instruments, the fixture that resets them
# and runs a program, and the expectations about what they recorded. An object library, because
# the global fixture of dut.cpp registers itself at static initialisation and an archive would
# drop the object nothing refers to.
add_library(scheduled_radio_test_tools OBJECT
    dut.cpp
    environment.cpp
    observations.cpp
    records.cpp
    tester.cpp
    timeline.cpp
)

add_library(scheduled_radio::test_tools ALIAS scheduled_radio_test_tools)
target_include_directories(scheduled_radio_test_tools SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
target_include_directories(scheduled_radio_test_tools PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(scheduled_radio_test_tools PUBLIC bluetoe::iface bluetoe::sm bluetoe::utility scheduled_radio::host)
