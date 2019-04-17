#include <hal/io.hpp>
#include <nrf.h>

static constexpr int io_pin = 21;

void init_led()
{
    // Init GPIO pin
    NRF_GPIO->PIN_CNF[ io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );
}

void set_led( bool state )
{
    NRF_GPIO->OUT = state
        ? NRF_GPIO->OUT | ( 1 << io_pin )
        : NRF_GPIO->OUT & ~( 1 << io_pin );
}
