find_package(Boost REQUIRED)

# A radio test needs a device under test on the serial port BLUETOE_DUT names. The label keeps
# ctest from running it unless asked for: `ctest -L radio` runs the radio tests only, the CI
# runs `ctest -LE radio` (decision 21). The target is prefixed with radio_, since the library's own
# tests take the plain names, advertising_tests among them.
function(add_radio_test name)
    set(test_runner radio_${name})
    add_executable(${test_runner} ${name}.cpp dut.cpp tester.cpp environment.cpp observations.cpp)

    target_include_directories(${test_runner} SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
    target_include_directories(${test_runner} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(${test_runner} PRIVATE bluetoe::iface bluetoe::sm bluetoe::utility scheduled_radio::host test::tools)
    set_target_properties(${test_runner} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

    add_test(${test_runner} ./${test_runner})
    set_tests_properties(${test_runner} PROPERTIES LABELS radio)
endfunction()

add_radio_test(toolbox_tests)
add_radio_test(reset_tests)
add_radio_test(advertising_tests)
add_radio_test(timer_tests)
add_radio_test(cancel_tests)
add_radio_test(connection_tests)
