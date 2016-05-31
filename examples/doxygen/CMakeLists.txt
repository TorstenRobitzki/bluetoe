function(add_doxygen_example TARGETNAME)
    add_library(${TARGETNAME} ${TARGETNAME}.cpp)
    add_dependencies(build_examples ${TARGETNAME})
endfunction()

add_custom_target(build_examples)

add_doxygen_example(server_example)
add_doxygen_example(include_example)
add_doxygen_example(read_write_handler_example)
add_doxygen_example(change_advertising_example)