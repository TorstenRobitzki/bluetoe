set(CMAKE_SYSTEM_NAME       Generic)
set(CMAKE_SYSTEM_PROCESSOR  arm)

set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GCC_TOOL_PATH)

if (NOT DEFINED ARM_GCC_TOOL_PATH OR NOT EXISTS ${ARM_GCC_TOOL_PATH}/bin)
    find_program(
        ARM_NONE_EABI_GCC_PATH
        arm-none-eabi-gcc
        REQUIRED)

    if (DEFINED ARM_NONE_EABI_GCC_PATH-NOTFOUND)
        message(FATAL_ERROR
                "To configure the arm-none-eabi-gcc correctly, "
                "please set ARM_GCC_TOOL_PATH to the path that "
                "contains the bin directory of your GCC installation.")
    endif()

    get_filename_component(ARM_NONE_EABI_GCC_PATH_BASE ${ARM_NONE_EABI_GCC_PATH} DIRECTORY)

    set(tools_path ${ARM_NONE_EABI_GCC_PATH_BASE})
else()
    set(tools_path ${ARM_GCC_TOOL_PATH}/bin)
endif()

set(tools_prefix ${tools_path}/arm-none-eabi-)

set(CMAKE_C_COMPILER    ${tools_prefix}gcc)
set(CMAKE_CXX_COMPILER  ${tools_prefix}g++)
set(CMAKE_SIZE          ${tools_prefix}size)

# for init check
set(CMAKE_EXE_LINKER_FLAGS_INIT --specs=nosys.specs)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
