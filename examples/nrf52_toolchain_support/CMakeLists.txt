if(CMAKE_CROSSCOMPILING)
    # create an interface-only target to publish hardware specific options. Any target that
    # requires the hardware specific options need only to depend on this target
    add_library(toolchain_nrf52 INTERFACE)
    add_library(toolchain::nrf52 ALIAS toolchain_nrf52)
    target_include_directories(toolchain_nrf52 INTERFACE ${NRF5_SDK_ROOT}/components/device)
    target_include_directories(toolchain_nrf52 INTERFACE ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include)
    target_include_directories(toolchain_nrf52 INTERFACE .)
    target_compile_definitions(toolchain_nrf52 INTERFACE NRF52 NRF52832_XXAA)

    enable_language(ASM)
    add_library(startup_nrf52 STATIC
            gcc_startup_nrf52.s
            system_nrf52.c
            )
    target_link_libraries(startup_nrf52 PRIVATE toolchain::nrf52)
    add_library(startup::nrf52 ALIAS startup_nrf52)
    set_source_files_properties(
            gcc_startup_nrf52.s
            COMPILE_FLAGS "-x assembler-with-cpp")
    add_custom_target(linker_script ALL
            COMMAND ${CMAKE_C_COMPILER} -E -P -x c
            -MMD -MP -MT ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld -MF ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld
            -o ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld
            ${CMAKE_CURRENT_SOURCE_DIR}/nrf52.ld
            )
else()
message(error "")
endif()
