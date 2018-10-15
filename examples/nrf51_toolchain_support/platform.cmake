if(CMAKE_CROSSCOMPILING)
    # set global compile options that depend on hardware and must apply to
    # all targets of the project
    add_compile_options(-mcpu=cortex-m0 -mthumb)
    add_compile_options(-mabi=aapcs)
    # when cmake's 3.13 is released we'll replace this with add_linker_options()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mcpu=cortex-m0 -mthumb -mabi=aapcs")
else()
    message(error "")
endif()
