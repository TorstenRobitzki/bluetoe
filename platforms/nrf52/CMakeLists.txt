if(CMAKE_CROSSCOMPILING)
    if (NOT DEFINED NRF5_SDK_ROOT)
        message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
    endif()

    # create an interface-only target to publish hardware specific options. Any target that
    # requires the hardware specific options need only to depend on this target
    add_library(toolchain_nrf52 INTERFACE)
    add_library(toolchain::nrf52 ALIAS toolchain_nrf52)
    target_include_directories(toolchain_nrf52 INTERFACE ${NRF5_SDK_ROOT}/modules/nrfx/mdk)
    target_include_directories(toolchain_nrf52 INTERFACE ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include)
    target_include_directories(toolchain_nrf52 INTERFACE .)
#    target_compile_definitions(toolchain_nrf52 INTERFACE NRF52 NRF52832_XXAA)
    target_compile_definitions(toolchain_nrf52 INTERFACE NRF52820_XXAA)
    target_compile_definitions(toolchain_nrf52 INTERFACE $<$<CONFIG:Debug>:BLUETOE_NRF52_RADIO_DEBUG>)

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
            ${CMAKE_CURRENT_SOURCE_DIR}/nrf52.ld)

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
            COMMAND nrfjprog --chiperase --family NRF52 ${select_jlink_device} --program ${target}.hex --verify --reset
            VERBATIM
            )
        add_dependencies(${target}.flash ${target}.artifacts)
    endfunction()

else()
    message(FATAL_ERROR "Please set CMAKE_TOOLCHAIN_FILE to one of the tool-chain files in platforms/cmake!")
endif()
