function(add_scheduled_radio_test test_runner)
    add_executable(${test_runner} ${test_runner}.cpp)

    target_include_directories(${test_runner} SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
    target_link_libraries(${test_runner} PRIVATE bluetoe::iface bluetoe::sm bluetoe::utility scheduled_radio::link scheduled_radio::host)

    add_test(${test_runner} ./${test_runner})
endfunction()

add_scheduled_radio_test(concept_tests)
add_scheduled_radio_test(link_ring_buffer_tests)
add_scheduled_radio_test(link_serialize_tests)
add_scheduled_radio_test(link_frame_tests)
add_scheduled_radio_test(instrument_dispatcher_tests)
add_scheduled_radio_test(instrument_reported_queue_tests)
add_scheduled_radio_test(instrument_address_set_tests)
add_scheduled_radio_test(host_proxy_tests)
add_scheduled_radio_test(instrument_dut_rig_tests)
add_scheduled_radio_test(instrument_tester_rig_tests)
add_scheduled_radio_test(instrument_tester_program_tests)
add_scheduled_radio_test(instrument_program_tests)

# the toolbox test runs the software toolbox of the security manager tests behind the rig
add_scheduled_radio_test(instrument_toolbox_tests)
target_include_directories(instrument_toolbox_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(instrument_toolbox_tests PRIVATE test::tools)

add_scheduled_radio_test(host_transport_tests)
add_scheduled_radio_test(host_central_tests)

# the expectations a radio test writes, which need no instrument
add_scheduled_radio_test(test_tools_timeline_tests)
target_sources(test_tools_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../test_tools/timeline.cpp)

add_scheduled_radio_test(test_tools_records_tests)
target_sources(test_tools_records_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../test_tools/records.cpp)
