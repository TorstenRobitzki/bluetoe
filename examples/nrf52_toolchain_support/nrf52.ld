/* Linker script to configure memory regions. */

GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
  RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = 32K
}

#include "gcc_nrf5x_common.ld"
