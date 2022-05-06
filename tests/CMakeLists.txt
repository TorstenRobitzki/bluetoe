#set(Boost_DEBUG 1)
find_package( Boost REQUIRED )

add_compile_options(-Wall -pedantic -Wextra -Wfatal-errors)

if (CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL AppleClang)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

add_subdirectory(test_tools)

function(add_and_register_test test_runner)
    add_executable(${test_runner} ${test_runner}.cpp)

    target_include_directories(${test_runner} PRIVATE ${Boost_INCLUDE_DIR})
    target_link_libraries(${test_runner} PRIVATE bluetoe::iface bluetoe::sm bluetoe::utility test::tools)
    target_compile_features(${test_runner} PRIVATE cxx_std_11)

    if (BLUETOE_EXCLUDE_SLOW_TESTS)
        target_compile_definitions(${test_runner} PRIVATE BLUETOE_EXCLUDE_SLOW_TESTS)
    endif()

    add_test(${test_runner} ./${test_runner})
endfunction()

function(bluetoe_add_test test_runner)
    add_and_register_test(${test_runner} bluetoe::iface)
endfunction()

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
add_and_register_test(attribute_handle_tests)
add_and_register_test(l2cap_tests)
add_and_register_test(notification_queue_tests)
add_and_register_test(bits_tests)

add_subdirectory(att)
add_subdirectory(link_layer)
add_subdirectory(services)
add_subdirectory(security_manager)
add_subdirectory(hci)
