/**
 * @example thermometer.cpp
 *
 * This example demonstrates, how notifications work in Bluetoe. To send out a notification
 * one of the server::notify() functions has to be called. Bluetoe uses then the existing read
 * handler to send out the notification.
 *
 * The example also uses a connection callback to set the connection to 2MBit.
 */

#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>

#include "resources.hpp"

std::int32_t temperature_value = 0x12345678;
static constexpr char server_name[] = "Temperature";
static constexpr char char_name[] = "Temperature Value";

using small_temperature_service = bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xF0E6EBE6, 0x3749, 0x41A6, 0xB190, 0x591B262AC20A >,
            bluetoe::no_write_access,
            bluetoe::notify,
            bluetoe::characteristic_name< char_name >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >
        >
    >
>;

void start_temperatur_messurement();

struct callbacks_t {
    template < typename ConnectionData >
    void ll_connection_established(
        const bluetoe::link_layer::connection_details&   details,
        const bluetoe::link_layer::connection_addresses& addresses,
              ConnectionData&                            connection );

} callbacks;

bluetoe::device<
    small_temperature_service,
    bluetoe::link_layer::advertising_interval< 250u >,
    bluetoe::link_layer::static_address< 0xf0, 0xa6, 0xd5, 0x4f, 0x60, 0x0b >,
    bluetoe::link_layer::connection_callbacks< callbacks_t, callbacks >
> server;

template < typename ConnectionData >
void callbacks_t::ll_connection_established(
    const bluetoe::link_layer::connection_details&   ,
    const bluetoe::link_layer::connection_addresses& ,
          ConnectionData&                             )
{
    server.phy_update_request_to_2mbit();
}

static examples::temperature temperature_sensor;

int main()
{

    for ( ; ; )
    {
        server.run();

        temperature_sensor.handle_event();

        const std::int32_t new_value = temperature_sensor.value();
        const auto delta = std::abs( new_value - temperature_value );

        if ( delta >= 3 )
        {
            temperature_value = new_value;
            server.notify( temperature_value );
        }
    }
}
