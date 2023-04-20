#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <nrf.h>

#include "resources.hpp"

using namespace bluetoe;

// mapping to LED defined in resources.hpp
static examples::led output;

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

struct oob_cb_t {
    std::pair< bool, std::array< std::uint8_t, 16 > > sm_oob_authentication_data(
        const bluetoe::link_layer::device_address& /* address */ )
    {
        return { true, std::array< std::uint8_t, 16 >{{
            0xF1, 0x50, 0xA0, 0xAE,
            0xB7, 0xAA, 0xBA, 0xC8,
            0x19, 0x22, 0xB6, 0x15,
            0x4C, 0x23, 0x94, 0x7A
        }} };
    }

} oob_cb;

device<
    blinky_server,
    link_layer::buffer_sizes< 200, 200 >,
    legacy_security_manager, // use one of legacy_security_manager, lesc_security_manager or security_manager
    oob_authentication_callback< oob_cb_t, oob_cb >
> gatt_srv;

int main()
{
    for ( ;; )
        gatt_srv.run();
}
