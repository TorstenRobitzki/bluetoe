set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GCC_TOOL_PATH)

if (NOT DEFINED ARM_GCC_TOOL_PATH)
    message(FATAL_ERROR "To configure the arm-none-eabi-gcc correctly, please set ARM_GCC_TOOL_PATH to the path that contains the bin directory of your GCC installation.")
elseif(NOT EXISTS ${tools}/bin)
    message(FATAL_ERROR "To configure the arm-none-eabi-gcc correctly, please set ARM_GCC_TOOL_PATH to the path that contains the bin directory of your GCC installation.")
endif()

set(CMAKE_SYSTEM_PROCESSOR arm)
set(tools ${ARM_GCC_TOOL_PATH})
set(tools_prefix arm-none-eabi-)
set(CMAKE_C_COMPILER ${tools}/bin/${tools_prefix}gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/${tools_prefix}g++)
set(CMAKE_SIZE ${tools}/bin/${tools_prefix}size)

# for init check
set(CMAKE_EXE_LINKER_FLAGS_INIT --specs=nosys.specs)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
