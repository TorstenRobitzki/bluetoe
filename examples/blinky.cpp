#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include "hal/io.hpp"

using namespace bluetoe;

static std::uint8_t io_pin_write_handler( bool state )
{
    // the GPIO pin according to the received value: 0 = off, 1 = on
    set_led( state );

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

device< blinky_server > gatt_srv;

int main()
{
    init_led();

    for ( ;; )
        gatt_srv.run( gatt );
}
