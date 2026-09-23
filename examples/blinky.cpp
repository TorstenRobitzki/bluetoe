/**
 * @example blinky.cpp
 *
 * This example shows, how to implement a very simple GATT server that
 * provides one service to switch an LED.
 *
 * The example servers only characteristic requires encryption. By default,
 * the server implements only LESC pairing. Bonding is not implemented in
 * this example.
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
            requires_encryption,
            free_write_handler< bool, io_pin_write_handler >
        >
    >,
    max_mtu_size< 65 >
>;

/*
 * The sleep clock of the radio: the default, synthesized from the high frequency crystal,
 * or the one CMake names for the variants the power figures are measured on.
 */
#if defined( BLUETOE_EXAMPLE_SLEEP_CLOCK )
    using sleep_clock = bluetoe::nrf::BLUETOE_EXAMPLE_SLEEP_CLOCK;
#else
    using sleep_clock = bluetoe::nrf::synthesized_sleep_clock;
#endif

device<
    blinky_server,
    sleep_clock,
    link_layer::buffer_sizes< 200, 200 >
> gatt_srv;

int main()
{
    for ( ;; )
        gatt_srv.run();
}
