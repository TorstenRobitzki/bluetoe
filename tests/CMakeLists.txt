#set(Boost_DEBUG 1)
find_package( Boost REQUIRED )
if (Boost_FOUND)
    include_directories(BEFORE ${Boost_INCLUDE_DIR})
endif()

add_custom_target(all_tests)
add_custom_target(all_tests.run)

add_library(test_tools
    ${CMAKE_SOURCE_DIR}/tests/test_uuid.cpp
    ${CMAKE_SOURCE_DIR}/tests/hexdump.cpp
    ${CMAKE_SOURCE_DIR}/tests/link_layer/test_radio.cpp
    ${CMAKE_SOURCE_DIR}/tests/link_layer/buffer_io.cpp)

function(add_test TARGETNAME)
    add_executable(${TARGETNAME} ${TARGETNAME}.cpp ${ARGV1} ${ARGV2} ${ARGV3} ${ARGV4})
    target_link_libraries(${TARGETNAME} bluetoe test_tools)
    add_custom_target(
        ${TARGETNAME}.run
        COMMAND ${TARGETNAME})

    add_dependencies(all_tests ${TARGETNAME})
    add_dependencies(all_tests.run ${TARGETNAME}.run)
endfunction()

if (BLUETOE_EXCLUDE_SLOW_TESTS)
    add_definitions(-DBLUETOE_EXCLUDE_SLOW_TESTS)
    message(WARNING "excluding slow tests from test set!")
endif()

add_test(write_queue_tests)
add_test(service_tests)
add_test(options_tests)
add_test(characteristic_tests)
add_test(characteristic_value_tests)
add_test(advertising_tests)
add_test(filter_tests)
add_test(server_tests)
add_test(auto_uuid_tests)
add_test(scattered_access_tests)
add_test(gap_service_tests)
add_test(read_write_handler_tests)

add_subdirectory(att)
add_subdirectory(link_layer)
add_subdirectory(services)
