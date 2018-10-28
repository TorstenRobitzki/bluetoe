/* Linker script to configure memory regions. */

GROUP(-lgcc -lc -lnosys)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x40000
    NRF_UICR (rw)       : ORIGIN = 0x10001000, LENGTH = 256
    RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = 0x4000
}

PROVIDE(__flash_start__ = ORIGIN(FLASH));
PROVIDE(__flash_length__ = LENGTH(FLASH));
PROVIDE(__flash_end__ = ORIGIN(FLASH) + LENGTH(FLASH));

#include "gcc_nrf51_common.ld"

