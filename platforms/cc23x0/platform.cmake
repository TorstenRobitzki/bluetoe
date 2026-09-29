if(CMAKE_CROSSCOMPILING)
    # the Cortex-M0+ of the CC2340, applied to every target of the project
    add_compile_options(-mcpu=cortex-m0plus -mthumb -mabi=aapcs -mfloat-abi=soft)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mcpu=cortex-m0plus -mthumb -mabi=aapcs -mfloat-abi=soft")
else()
    message(error "")
endif()
