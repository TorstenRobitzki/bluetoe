#include <server.hpp>
#include <nrf51.hpp>
#include <nrf.h>

using namespace bluetoe;

static constexpr int io_pin = 21;

static std::uint8_t io_pin_write_handler( bool state )
{
    // the GPIO pin according to the received value: 0 = off, 1 = on
    NRF_GPIO->OUT = state
        ? NRF_GPIO->OUT | ( 1 << io_pin )
        : NRF_GPIO->OUT & ~( 1 << io_pin );

    return error_codes::success;
}

typedef server<
    service<
        service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        characteristic<
            free_write_handler< bool, io_pin_write_handler >
        >
    >
> blinky_server;

blinky_server gatt;

nrf51< blinky_server > gatt_srv;

int main()
{
    // Init GPIO pin
    NRF_GPIO->PIN_CNF[ io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    for ( ;; )
        gatt_srv.run( gatt );
}
