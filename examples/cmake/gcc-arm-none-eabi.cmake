set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(tools /usr/local/gcc-arm-none-eabi-6-2017-q2-update)
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
