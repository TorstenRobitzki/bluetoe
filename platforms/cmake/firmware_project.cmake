# Included by a firmware project before project(): selects the cross toolchain and checks the
# cache variables that name the hardware. The examples and the test rigs of the scheduled radio
# are such projects; after project(), they include firmware.cmake.
#
# BLUETOE_BOARD names one of the supported evaluation boards, from which the binding follows;
# BLUETOE_BINDING names the microcontroller directly for custom hardware. Exactly one is set.

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(AVAILABLE_BOARDS PCA10056 PCA10040)
set(AVAILABLE_BINDINGS NRF52840 NRF52833 NRF52832 NRF52820 NRF52811 NRF52810 NRF52805)

include(${CMAKE_CURRENT_LIST_DIR}/deduce.cmake)

if (DEFINED BLUETOE_BOARD)
    set_property(CACHE BLUETOE_BOARD PROPERTY STRINGS ${AVAILABLE_BOARDS})

    if (DEFINED BLUETOE_BINDING)
        message(FATAL_ERROR "'BLUETOE_BOARD' is set to '${BLUETOE_BOARD}' while 'BLUETOE_BINDING' is set to '${BLUETOE_BINDING}'. It is not expected to have both variables beeing set!")
    endif()

    if (NOT ${BLUETOE_BOARD} IN_LIST AVAILABLE_BOARDS)
        message(FATAL_ERROR "'BLUETOE_BOARD' is set to '${BLUETOE_BOARD}', which is an invalid value. Supported values are: ${AVAILABLE_BOARDS}")
    endif()

    deduce_binding(BLUETOE_BINDING ${BLUETOE_BOARD})
else()
    if (NOT DEFINED BLUETOE_BINDING)
        message(FATAL_ERROR "For a custom hardware, the 'BLUETOE_BINDING' cache variable is expected to be set.")
    endif()

    if (NOT ${BLUETOE_BINDING} IN_LIST AVAILABLE_BINDINGS)
        message(FATAL_ERROR "'BLUETOE_BINDING' is set to '${BLUETOE_BINDING}', which is an invalid value. Supported values are: ${AVAILABLE_BINDINGS}")
    endif()

    set_property(CACHE BLUETOE_BINDING PROPERTY STRINGS ${AVAILABLE_BINDINGS})
endif()

# Currently every firmware is compiled with arm-none-eabi-gcc
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/gcc-arm-none-eabi.cmake)
