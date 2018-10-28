if(CMAKE_CROSSCOMPILING)
    # create an interface-only target to publish hardware specific options. Any target that
    # requires the hardware specific options need only to depend on this target
    add_library(toolchain_nrf51 INTERFACE)
    add_library(toolchain::nrf51 ALIAS toolchain_nrf51)
    target_include_directories(toolchain_nrf51 INTERFACE ${NRF5_SDK_ROOT}/components/device)
    target_include_directories(toolchain_nrf51 INTERFACE ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include)
    target_include_directories(toolchain_nrf51 INTERFACE .)
    target_compile_definitions(toolchain_nrf51 INTERFACE NRF51)

    enable_language(ASM)
    add_library(startup_nrf51 STATIC
            gcc_startup_nrf51.s
            system_nrf51422.c
            )
    target_link_libraries(startup_nrf51 PRIVATE toolchain::nrf51)
    add_library(startup::nrf51 ALIAS startup_nrf51)
    set_source_files_properties(
            gcc_startup_nrf51.s
            COMPILE_FLAGS "-x assembler-with-cpp")
    add_custom_target(linker_script ALL
            COMMAND ${CMAKE_C_COMPILER} -E -P -x c
            -MMD -MP -MT ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld -MF ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld
            -o ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld
            ${CMAKE_CURRENT_SOURCE_DIR}/nrf51_xxaa.ld
            )
else()
message(error "")
endif()
