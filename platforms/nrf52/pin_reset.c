/*
 * An nRF52 treats its reset pin as a reset only while UICR.PSELRESET[0] and [1] both name the
 * pin, and a chip erase leaves the UICR blank. These two words are placed at the UICR address
 * by the linker script and programmed together with the image, so that the reset button of
 * an evaluation board and the reset line of the scheduled radio tester work from the first
 * boot on, without the startup code writing flash and resetting itself.
 *
 * The pin differs between the parts of the family, which is why CMakeLists.txt defines
 * BLUETOE_RESET_PIN from the binding.
 */
#include <nrf.h>

#include <stdint.h>

__attribute__(( section( ".uicr_pselreset" ), used ))
const uint32_t uicr_pselreset[ 2 ] =
{
    ( BLUETOE_RESET_PIN << UICR_PSELRESET_PIN_Pos ) | ( UICR_PSELRESET_CONNECT_Connected << UICR_PSELRESET_CONNECT_Pos ),
    ( BLUETOE_RESET_PIN << UICR_PSELRESET_PIN_Pos ) | ( UICR_PSELRESET_CONNECT_Connected << UICR_PSELRESET_CONNECT_Pos )
};
