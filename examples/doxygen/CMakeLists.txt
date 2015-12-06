function(add_doxygen_example TARGETNAME)
    add_library(${TARGETNAME} ${TARGETNAME}.cpp)
    add_custom_target(build_examples DEPENDS ${TARGETNAME})
endfunction()

add_doxygen_example(server server.cpp)
