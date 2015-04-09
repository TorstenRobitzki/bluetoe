#set(Boost_DEBUG 1)
find_package( Boost REQUIRED )
if (Boost_FOUND)
    include_directories(BEFORE ${Boost_INCLUDE_DIR})
endif()

add_custom_target(all_tests)
add_custom_target(all_tests.run)

function(add_test TARGETNAME)
    add_executable(${TARGETNAME} ${TARGETNAME}.cpp ${ARGV1} ${ARGV2} ${ARGV3} ${ARGV4})
    add_custom_target(
        ${TARGETNAME}.run
        COMMAND ${TARGETNAME})

    add_dependencies(all_tests ${TARGETNAME})
    add_dependencies(all_tests.run ${TARGETNAME}.run)
endfunction()

add_test(find_information_att_tests)