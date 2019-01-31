if(CMAKE_CROSSCOMPILING)
    if (NOT DEFINED NRF5_SDK_ROOT)
        message(FATAL_ERROR "Please define NRF5_SDK_ROOT to point to your Nordic SDK installation!")
    endif()

    # create an interface-only target to publish hardware specific options. Any target that
    # requires the hardware specific options need only to depend on this target
    add_library(toolchain_nrf52 INTERFACE)
    add_library(toolchain::nrf52 ALIAS toolchain_nrf52)
    target_include_directories(toolchain_nrf52 INTERFACE ${NRF5_SDK_ROOT}/components/device)
    target_include_directories(toolchain_nrf52 INTERFACE ${NRF5_SDK_ROOT}/components/toolchain/cmsis/include)
    target_include_directories(toolchain_nrf52 INTERFACE .)
    target_compile_definitions(toolchain_nrf52 INTERFACE NRF52 NRF52832_XXAA)

    add_compile_definitions($<$<CONFIG:Debug>:BLUETOE_NRF51_RADIO_DEBUG>)

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

    function(add_linker_script target)
        # As the function gets called from expamples/CMakeLists.txt CMAKE_CURRENT_LIST_DIR and CMAKE_CURRENT_SOURCE_DIR
        # is expamples/
        set_property(
            TARGET ${target}
            APPEND
            PROPERTY
                INTERFACE_LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/nrf52_toolchain_support/linker_script.ld)

        target_link_options(${target}
            PRIVATE -T ${CMAKE_CURRENT_BINARY_DIR}/nrf52_toolchain_support/linker_script.ld)

        add_dependencies(${target} linker_script)
    endfunction()

    if (DEFINED NRFJPROG_DEVICE)
        message( WARNING "nrfjprog ${NRFJPROG_DEVICE} selected")
        set(SELECT_JLINK_DEVICE --snr ${NRFJPROG_DEVICE} PARENT_SCOPE)
    endif()

    function(define_flash_command target)
        add_custom_target(${target}.flash
            COMMAND nrfjprog --chiperase --family NRF52 ${SELECT_JLINK_DEVICE} --program ${target}.hex --verify --reset
            VERBATIM
            )
        add_dependencies(${target}.flash ${target}.artifacts)
    endfunction()

else()
    message(FATAL_ERROR "Please set CMAKE_TOOLCHAIN_FILE to one of the tool-chain files in examples/cmake!")
endif()
