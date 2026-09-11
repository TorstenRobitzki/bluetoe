function(add_scheduled_radio_test test_runner)
    add_executable(${test_runner} ${test_runner}.cpp)

    target_include_directories(${test_runner} SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
    target_link_libraries(${test_runner} PRIVATE bluetoe::iface bluetoe::sm bluetoe::utility scheduled_radio::link)
    set_target_properties(${test_runner} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

    add_test(${test_runner} ./${test_runner})
endfunction()

add_scheduled_radio_test(concept_tests)
add_scheduled_radio_test(link_ring_buffer_tests)
add_scheduled_radio_test(link_serialize_tests)
add_scheduled_radio_test(link_frame_tests)
add_scheduled_radio_test(instrument_dispatcher_tests)
add_scheduled_radio_test(host_proxy_tests)
add_scheduled_radio_test(instrument_dut_rig_tests)

# the toolbox test runs the software toolbox of the security manager tests behind the rig
add_scheduled_radio_test(instrument_toolbox_tests)
target_include_directories(instrument_toolbox_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(instrument_toolbox_tests PRIVATE test::tools)
