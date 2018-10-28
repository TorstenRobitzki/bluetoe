if(CMAKE_CROSSCOMPILING)
    # set global compile options that depend on hardware and must apply to
    # all targets of the project
    add_compile_options(-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=hard -mfpu=fpv4-sp-d16)
    # when cmake's 3.13 is released we'll replace this with add_linker_options()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=hard -mfpu=fpv4-sp-d16")
else()
    message(error "")
endif()
