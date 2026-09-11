# Included by a firmware project after project(): applies the options every Bluetoe firmware is
# built with, adds the platform support and the library, and defines add_bluetoe_firmware().
# The options are applied here, in the project's own scope, so that they reach the library and
# every subdirectory the project adds.

deduce_bluetoe_binding(BINDING ${BLUETOE_BINDING})

message("BLUETOE_BOARD: ${BLUETOE_BOARD}")
message("BLUETOE_BINDING: ${BLUETOE_BINDING}")
message("BINDING: ${BINDING}")

# hardware specific compile options and definitions that must be applied to the whole project
include(${CMAKE_CURRENT_LIST_DIR}/../${BINDING}/platform.cmake)

# set global compile options that are hardware independent, these will be used to build the applications
# and will also be used to build bluetoe library
# Put every function and object into it's own section and ask the linker to remove unreferenced sections
add_compile_options(-ffunction-sections -fdata-sections)
add_link_options(LINKER:--gc-sections LINKER:--warn-common)

# add global options to cpp targets
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ftemplate-backtrace-limit=0>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)

# Optimizations / Debug
add_compile_options($<IF:$<CONFIG:Debug>,-O0,-Os>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# Regardles of build type: Debug informations just make it into the elf file, never into the final binary
add_compile_options(-g)
add_link_options(-g)

# Standard libraries
add_compile_options(-nostdlib)
add_link_options(-lm -lstdc++ -lsupc++ -nostdlib --specs=nano.specs -static)

# the platform support: assert(), the C++ runtime, the binding's toolchain, startup and linker script
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_BINARY_DIR}/platforms)

# bluetoe library
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. ${CMAKE_BINARY_DIR}/bluetoe)

# Additional settings for the Bluetoe library itself:
target_link_libraries(bluetoe_iface INTERFACE assert::arm)

# A firmware: an executable from the given sources with the runtime, the startup code, the
# linker script and the binding; <target>.artifacts produces hex, bin and listing and prints the
# size, <target>.flash programs it if BLUETOE_JLINK is set.
function(add_bluetoe_firmware target_name)
    add_executable(${target_name} ${ARGN})
    set_target_properties(${target_name}
        PROPERTIES
            OUTPUT_NAME ${target_name}.elf)

    add_linker_script(${target_name})

    target_link_libraries(${target_name}
        PUBLIC
            assert::arm
        PRIVATE
            runtime::gcc
            toolchain::${BINDING}
            startup::${BINDING}
            bluetoe::bindings::${BINDING}
    )

    add_custom_target(${target_name}.artifacts ALL
            COMMAND ${CMAKE_OBJCOPY} -S -O ihex ${target_name}.elf ${target_name}.hex
            COMMAND ${CMAKE_OBJCOPY} -S -O binary --only-section=.text ${target_name}.elf ${target_name}.bin
            COMMAND ${CMAKE_OBJDUMP} -hS ${target_name}.elf > ${target_name}.lss
            COMMAND ${CMAKE_SIZE} ${target_name}.elf
            )
    add_dependencies(${target_name}.artifacts ${target_name})

    define_flash_command(${target_name})
endfunction()
