# The scheduled radio 2 work is not bound to C++11 (documentation/scheduled_radio_test_rig.md,
# decision 13). Its targets set their own standard rather than inheriting the C++11 pin the
# continuous integration build applies to the rest of the library.
function(add_scheduled_radio_test test_runner)
    add_executable(${test_runner} ${test_runner}.cpp)

    target_include_directories(${test_runner} SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
    target_link_libraries(${test_runner} PRIVATE bluetoe::iface bluetoe::sm bluetoe::utility bluetoe::link_layer)
    set_target_properties(${test_runner} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

    add_test(${test_runner} ./${test_runner})
endfunction()

add_scheduled_radio_test(concept_tests)
