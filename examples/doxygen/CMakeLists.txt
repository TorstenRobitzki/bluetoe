function(add_doxygen_example TARGETNAME)
    add_library(${TARGETNAME} ${TARGETNAME}.cpp)
    add_dependencies(build_examples ${TARGETNAME})
endfunction()

add_custom_target(build_examples)

add_doxygen_example(server server.cpp)
add_doxygen_example(include_example include_example.cpp)
