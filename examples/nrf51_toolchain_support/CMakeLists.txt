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

    function(add_linker_script target)
        # As the function gets called from expamples/CMakeLists.txt CMAKE_CURRENT_LIST_DIR and CMAKE_CURRENT_SOURCE_DIR
        # is expamples/
        set_property(
            TARGET ${target}
            APPEND
            PROPERTY
                INTERFACE_LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/nrf51_toolchain_support/linker_script.ld)

        target_link_options(${target}
            PRIVATE -T ${CMAKE_CURRENT_BINARY_DIR}/nrf51_toolchain_support/linker_script.ld)

        add_dependencies(${target} linker_script)
    endfunction()

    function(define_flash_command target)
        add_custom_target(${target}.flash
            COMMAND nrfjprog --chiperase --family NRF51 --program ${target}.hex
            VERBATIM
            )
        add_dependencies(${target}.flash ${target}.artifacts)
    endfunction()

else()
    message(FATAL_ERROR "Please set CMAKE_TOOLCHAIN_FILE to one of the tool-chain files in examples/cmake!")
endif()
