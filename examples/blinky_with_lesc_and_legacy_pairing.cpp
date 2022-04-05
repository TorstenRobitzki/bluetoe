#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <nrf.h>

using namespace bluetoe;

// LED1 on a nRF52 eval board
static constexpr int io_pin = 13;

static std::uint8_t io_pin_write_handler( bool state )
{
    // on an nRF52 eval board, the pin is connected to the LED's cathode, this inverts the logic.
    NRF_GPIO->OUT = state
        ? NRF_GPIO->OUT & ~( 1 << io_pin )
        : NRF_GPIO->OUT | ( 1 << io_pin );

    return error_codes::success;
}

using blinky_server = server<
    service<
        service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        characteristic<
            requires_encryption,
            free_write_handler< bool, io_pin_write_handler >
        >
    >,
    max_mtu_size< 65 >
>;

device<
    blinky_server,
    security_manager,
    link_layer::buffer_sizes< 200, 200 > > gatt_srv;

int main()
{
    // Init GPIO pin
    NRF_GPIO->PIN_CNF[ io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    for ( ;; )
        gatt_srv.run();
}
