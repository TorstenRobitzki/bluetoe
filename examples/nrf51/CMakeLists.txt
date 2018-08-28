cmake_minimum_required(VERSION 2.8.12)

set(CXX_DEFAULT_FLAGS "-Wall")
set(CXX_SPECIFIC_FLAGS "-std=c++11")
set(C_SPECIFIC_FLAGS "-std=c99")

set(CXX_DISABLED_WARNINGS "-Wno-parentheses")

# Stop compilation after first error
set(CXX_DEFAULT_FLAGS "${CXX_DEFAULT_FLAGS} -Wfatal-errors -Werror -Wno-sign-compare")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CXX_COMPILER "arm-none-eabi-g++")
set(CMAKE_C_COMPILER "arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "arm-none-eabi-gcc")
set(CMAKE_C_COMPILER_FORCED 1)
set(CMAKE_CXX_COMPILER_FORCED 1)
set(ARM_FLAGS "--sysroot=/usr/local/arm/arm-none-eabi -mthumb -mabi=aapcs -mcpu=cortex-m4")
set(ARM_FLAGS "${ARM_FLAGS} -ffunction-sections -fdata-sections")
set(CXX_ARC_FLAGS "${ARM_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(LINK_ARC_FLAGS "${ARM_FLAGS} --specs=nosys.specs --specs=nano.specs -static -lm -lstdc++ -lsupc++")
set(CXX_SPECIFIC_FLAGS "${CXX_SPECIFIC_FLAGS} -ftemplate-backtrace-limit=0 -fvisibility-inlines-hidden -fno-rtti -fno-exceptions")
set(CXX_DISABLED_WARNINGS "${CXX_DISABLED_WARNINGS} -Wno-unused-local-typedefs")
set(LINK_DEFAULT_FLAGS "${LINK_DEFAULT_FLAGS} -Wl,--gc-sections -L ${CMAKE_SOURCE_DIR}/examples -nostdlib")

# debugging
set(CXX_BUILD_TYPE_FLAGS "-g -Os")
set(LINK_BUILD_TYPE_FLAGS "-g")

project(bluetoe_nrf51_examples CXX ASM C)

set(CMAKE_CXX_FLAGS "${CXX_DEFAULT_FLAGS} ${CXX_SPECIFIC_FLAGS} ${CXX_ARC_FLAGS} ${CXX_BUILD_TYPE_FLAGS} ${CXX_DISABLED_WARNINGS}")
set(CMAKE_C_FLAGS   "${CXX_DEFAULT_FLAGS} ${CXX_ARC_FLAGS} ${CXX_BUILD_TYPE_FLAGS} ${CXX_DISABLED_WARNINGS}")
set(CMAKE_EXE_LINKER_FLAGS "${LINK_DEFAULT_FLAGS} ${LINK_ARC_FLAGS} ${LINK_BUILD_TYPE_FLAGS} -L ${CMAKE_SOURCE_DIR}")

#add_definitions(-D_LITTLE_ENDIAN=1 -DNRF51 -D__HEAP_SIZE=0)
add_definitions(-D_LITTLE_ENDIAN=1)
add_definitions(-DNRF52 -DNRF52832_XXAA)
add_definitions(-DBOARD_PCA10040)
add_definitions(-D__HEAP_SIZE=0)
#add_definitions(-DNDEBUG=1)
add_definitions(-DBLUETOE_NRF51_RADIO_DEBUG=1)

set(BLUETOE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include_directories(${NRF5_SDK_ROOT}/components/device)
include_directories(${NRF5_SDK_ROOT}/components/toolchain)
include_directories(${NRF5_SDK_ROOT}/components/toolchain/gcc)
include_directories(${NRF5_SDK_ROOT}/components/toolchain/cmsis/include/)
include_directories(${BLUETOE_ROOT})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(SYSTEM ./assert)

if (DEFINED NRFJPROG_DEVICE)
    message( WARNING "nrfjprog ${NRFJPROG_DEVICE} selected")
    set(SELECT_JLINK_DEVICE --snr ${NRFJPROG_DEVICE})
endif()

function(add_example TARGETNAME)
    add_executable(${TARGETNAME}.elf
        ${TARGETNAME}.cpp
        ${BLUETOE_ROOT}/bluetoe/link_layer/delta_time.cpp
        ${BLUETOE_ROOT}/bluetoe/link_layer/address.cpp
        ${BLUETOE_ROOT}/bluetoe/link_layer/channel_map.cpp
        ${BLUETOE_ROOT}/bluetoe/link_layer/connection_details.cpp
        ${BLUETOE_ROOT}/bluetoe/bindings/nrf51.cpp
        runtime.cpp
        assert/assert.cpp
        ${NRF5_SDK_ROOT}/components/toolchain/system_nrf52.c
        gcc_startup_nrf52.s
    )

    set_target_properties(${TARGETNAME}.elf PROPERTIES
        LINK_FLAGS "-T ${CMAKE_CURRENT_LIST_DIR}/nrf52.ld -Wl,-Map -Wl,${TARGETNAME}.map")

    add_custom_target(${TARGETNAME}.bin
        arm-none-eabi-objcopy -S -O binary ${TARGETNAME}.elf ${TARGETNAME}.bin
        )
    add_custom_target(${TARGETNAME}.hex
        arm-none-eabi-objcopy -S -O ihex ${TARGETNAME}.elf ${TARGETNAME}.hex
        )
    add_dependencies(${TARGETNAME}.bin ${TARGETNAME}.elf)
    add_dependencies(${TARGETNAME}.hex ${TARGETNAME}.elf)

    add_custom_target(${TARGETNAME}.flash
        COMMAND nrfjprog --family NRF52 --sectorerase ${SELECT_JLINK_DEVICE} --program ${TARGETNAME}.hex --verify --reset
        )

    add_dependencies(${TARGETNAME}.flash ${TARGETNAME}.hex)
endfunction()

add_custom_target(
    erase.flash
    COMMAND nrfjprog --eraseall --family NRF52 ${SELECT_JLINK_DEVICE})

add_example(thermometer)
add_example(cycling_speed_and_cadence)
add_example(blinky)
add_example(blinky_without_encryption)
add_example(bootloader)

set_target_properties(bootloader.elf PROPERTIES
    LINK_FLAGS "-T ${CMAKE_CURRENT_LIST_DIR}/bootloader.ld -Wl,-Map -Wl,bootloader.map")
