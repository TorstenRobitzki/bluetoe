/* Linker script to configure memory regions.
 *
 * The memory of the smallest part of the family, so that a firmware that links proves it fits
 * every nRF52. A firmware that is an instrument rather than a product, and is tied to one
 * board, sets BLUETOE_RAM_SIZE to what that board has; see platforms/README.md.
 */

GROUP(-lgcc -lc -lnosys)

#ifndef BLUETOE_RAM_SIZE
#   define BLUETOE_RAM_SIZE 32K
#endif

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
  RAM (rwx) :  ORIGIN = 0x20000000, LENGTH = BLUETOE_RAM_SIZE
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
