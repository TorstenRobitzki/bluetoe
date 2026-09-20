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

    # The reset pin of the part, for the UICR words in pin_reset.c
    set(reset_pin_mapping
        NRF52840 18
        NRF52833 18
        NRF52820 18
        NRF52832 21
        NRF52811 21
        NRF52810 21
        NRF52805 21)
    map_lookup(reset_pin reset_pin_mapping ${BLUETOE_BINDING})

    # An object library rather than an archive: the linker takes from an archive only what
    # resolves a reference, and nothing references the UICR words.
    enable_language(ASM)
    add_library(startup_nrf52 OBJECT
            gcc_startup_nrf52.s
            system_nrf52.c
            pin_reset.c
            )
    target_link_libraries(startup_nrf52 PRIVATE toolchain::nrf52)
    target_compile_definitions(startup_nrf52 PRIVATE BLUETOE_RESET_PIN=${reset_pin})
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
    set(BLUETOE_NRF52_LINKER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/nrf52.ld CACHE INTERNAL "the linker script a firmware's own memory is generated from")

    # A firmware that sets BLUETOE_RAM_SIZE before it is added gets a script of its own with
    # that much RAM; everything else links against the shared one and its conservative memory.
    function(add_linker_script target)
        get_property(script GLOBAL PROPERTY BLUETOE_LINKER_SCRIPT)

        if (DEFINED BLUETOE_RAM_SIZE)
            set(script ${CMAKE_CURRENT_BINARY_DIR}/linker_script_${target}.ld)

            add_custom_command(OUTPUT ${script}
                COMMAND ${CMAKE_C_COMPILER} -E -P -x c
                -DBLUETOE_RAM_SIZE=${BLUETOE_RAM_SIZE}
                -MMD -MP -MT ${script} -MF ${script}.d
                -o ${script}
                ${BLUETOE_NRF52_LINKER_SOURCE}
                DEPENDS ${BLUETOE_NRF52_LINKER_SOURCE})

            add_custom_target(${target}_linker_script DEPENDS ${script})
            add_dependencies(${target} ${target}_linker_script)
        else()
            add_dependencies(${target} linker_script)
        endif()

        set_property(
            TARGET ${target}
            APPEND
            PROPERTY
                INTERFACE_LINK_DEPENDS ${script})

        target_link_options(${target}
            PRIVATE -T ${script})
    endfunction()

    if (DEFINED BLUETOE_JLINK)
        message( WARNING "J-Link ${BLUETOE_JLINK} selected")
        set_property(GLOBAL PROPERTY BLUETOE_SELECT_JLINK_DEVICE --serial-number ${BLUETOE_JLINK})
    endif()

    function(define_flash_command target)
        get_property(select_jlink_device GLOBAL PROPERTY BLUETOE_SELECT_JLINK_DEVICE)

        add_custom_target(${target}.flash
            COMMAND nrfutil device program --firmware ${target}.hex ${select_jlink_device} --options chip_erase_mode=ERASE_ALL,verify=VERIFY_READ,reset=RESET_SYSTEM
            VERBATIM
            )
        add_dependencies(${target}.flash ${target}.artifacts)
    endfunction()

else()
    message(FATAL_ERROR "Please set CMAKE_TOOLCHAIN_FILE to one of the tool-chain files in platforms/cmake!")
endif()
