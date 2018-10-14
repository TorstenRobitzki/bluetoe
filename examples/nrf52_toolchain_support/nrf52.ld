/* Linker script to configure memory regions. */

GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 512K
  RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = 64K
}

#include "gcc_nrf5x_common.ld"
