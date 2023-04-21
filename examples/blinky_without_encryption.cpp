/**
 * @example blinky_without_encryption.cpp
 *
 * This example shows, how to implement a very simple GATT server that
 * provides one service to switch an LED.
 *
 * The example servers does not require encryption, which automatically results
 * in Bluetoe dropping any support for encryption / pairing.
 */

#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>

#include "resources.hpp"

static examples::led output;

using namespace bluetoe;

static std::uint8_t io_pin_write_handler( bool state )
{
    output.value( state );

    return error_codes::success;
}

using blinky_server = server<
    service<
        service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        characteristic<
            free_write_handler< bool, io_pin_write_handler >
        >
    >
>;

device< blinky_server > gatt_srv;

int main()
{
    for ( ;; )
        gatt_srv.run();
}