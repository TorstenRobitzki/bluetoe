/* Linker script to configure memory regions. */

GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
  RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = 32K
  /* the two UICR words that make the reset pin a reset pin, see pin_reset.c */
  UICR_PSELRESET (r) : ORIGIN = 0x10001200, LENGTH = 8
}

#include "gcc_nrf5x_common.ld"

SECTIONS
{
  .uicr_pselreset :
  {
    KEEP(*(.uicr_pselreset))
  } > UICR_PSELRESET
}
