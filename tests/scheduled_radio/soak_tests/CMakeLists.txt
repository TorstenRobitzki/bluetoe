find_package(Boost REQUIRED)

# A soak test runs a connection against a device under test and the tester for minutes to
# days. The label keeps ctest from running it unless asked for with `ctest -L soak_tests`, and
# ctest's own limit of 1500 s would end it, so it has none; a run of days starts the executable
# itself (README.md). The target is prefixed like the radio tests'.
function(add_soak_test name)
    set(test_runner soak_${name})
    add_executable(${test_runner} ${name}.cpp)

    target_include_directories(${test_runner} SYSTEM PRIVATE ${Boost_INCLUDE_DIR})
    target_include_directories(${test_runner} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(${test_runner} PRIVATE scheduled_radio::test_tools test::tools)

    add_test(${test_runner} ./${test_runner})
    set_tests_properties(${test_runner} PROPERTIES LABELS soak_tests TIMEOUT 0)
    add_dependencies(soak_tests ${test_runner})
endfunction()

add_soak_test(connection_tests)
