#set(Boost_DEBUG 1)
find_package( Boost REQUIRED )
if (Boost_FOUND)
    include_directories(BEFORE ${Boost_INCLUDE_DIR})
endif()

add_library(test_tools
    ${CMAKE_SOURCE_DIR}/bluetoe/link_layer/delta_time.cpp
    ${CMAKE_SOURCE_DIR}/bluetoe/link_layer/address_io.cpp
    ${CMAKE_SOURCE_DIR}/tests/test_uuid.cpp
    ${CMAKE_SOURCE_DIR}/tests/test_servers.cpp
    ${CMAKE_SOURCE_DIR}/tests/hexdump.cpp
    ${CMAKE_SOURCE_DIR}/tests/link_layer/test_radio.cpp
    ${CMAKE_SOURCE_DIR}/tests/link_layer/buffer_io.cpp)

function(add_and_register_test TARGETNAME)
    add_executable(${TARGETNAME} ${TARGETNAME}.cpp ${ARGV1} ${ARGV2} ${ARGV3} ${ARGV4})
    target_link_libraries(${TARGETNAME} bluetoe test_tools)
    add_test(${TARGETNAME} ${TARGETNAME})
endfunction()

if (BLUETOE_EXCLUDE_SLOW_TESTS)
    add_definitions(-DBLUETOE_EXCLUDE_SLOW_TESTS)
    message(WARNING "excluding slow tests from test set!")
endif()

add_and_register_test(write_queue_tests)
add_and_register_test(service_tests)
add_and_register_test(options_tests)
add_and_register_test(characteristic_tests)
add_and_register_test(characteristic_value_tests)
add_and_register_test(advertising_tests)
add_and_register_test(filter_tests)
add_and_register_test(server_tests)
add_and_register_test(auto_uuid_tests)
add_and_register_test(scattered_access_tests)
add_and_register_test(gap_service_tests)
add_and_register_test(read_write_handler_tests)
add_and_register_test(encryption_tests)

add_subdirectory(att)
add_subdirectory(link_layer)
add_subdirectory(services)
add_subdirectory(security_manager)
