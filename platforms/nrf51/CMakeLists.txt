if(CMAKE_CROSSCOMPILING)
    if (NOT DEFINED NRF5_SDK_ROOT)
        message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
    endif()

    # create an interface-only target to publish hardware specific options. Any target that
    # requires the hardware specific options need only to depend on this target
    add_library(toolchain_nrf51 INTERFACE)
    add_library(toolchain::nrf51 ALIAS toolchain_nrf51)
    target_include_directories(toolchain_nrf51 INTERFACE ${NRF5_SDK_ROOT}/components/device)
    target_include_directories(toolchain_nrf51 INTERFACE ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include)
    target_include_directories(toolchain_nrf51 INTERFACE .)
    target_compile_definitions(toolchain_nrf51 INTERFACE NRF51)

    add_compile_definitions($<$<CONFIG:Debug>:BLUETOE_NRF51_RADIO_DEBUG>)

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
            COMMAND ${CMAKE_COMMAND} -E
            -MMD -MP -MT ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld -MF ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld
            -o ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld
            ${CMAKE_CURRENT_SOURCE_DIR}/nrf51_xxaa.ld
            )

    # The function is called from the firmware's directory, so the script's path is recorded
    # here, where it is known.
    set_property(GLOBAL PROPERTY BLUETOE_LINKER_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/linker_script.ld)

    function(add_linker_script target)
        get_property(script GLOBAL PROPERTY BLUETOE_LINKER_SCRIPT)

        set_property(
            TARGET ${target}
            APPEND
            PROPERTY
                INTERFACE_LINK_DEPENDS ${script})

        target_link_options(${target}
            PRIVATE -T ${script})

        add_dependencies(${target} linker_script)
    endfunction()

    if (DEFINED BLUETOE_JLINK)
        message( WARNING "nrfjprog ${BLUETOE_JLINK} selected")
        set_property(GLOBAL PROPERTY BLUETOE_SELECT_JLINK_DEVICE --snr ${BLUETOE_JLINK})
    endif()

    function(define_flash_command target)
        get_property(select_jlink_device GLOBAL PROPERTY BLUETOE_SELECT_JLINK_DEVICE)

        add_custom_target(${target}.flash
            COMMAND nrfjprog --chiperase --family NRF51 ${select_jlink_device} --program ${target}.hex --verify --reset
            VERBATIM
            )
        add_dependencies(${target}.flash ${target}.artifacts)
    endfunction()

else()
    message(FATAL_ERROR "Please set CMAKE_TOOLCHAIN_FILE to one of the tool-chain files in platforms/cmake!")
endif()
