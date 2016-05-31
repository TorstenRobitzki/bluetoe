#include <bluetoe/server.hpp>
#include <bluetoe/bindings/nrf52.hpp>

static std::uint8_t write_handler( bool flag );

using gatt = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0xD91F985A, 0x7F39, 0x4BBF, 0x8CA4, 0xCEB3DA28BB5D >,
        bluetoe::characteristic<
            bluetoe::free_write_handler< bool, write_handler >
        >
    >
>;

bluetoe::nrf52<
    gatt,
    bluetoe::link_layer::connectable_undirected_advertising,
    bluetoe::link_layer::connectable_directed_advertising
> link_layer;



static std::uint8_t write_handler( bool flag )
{
    // change the advertising type according to the give flag value
    if ( flag )
    {
        link_layer.change_advertising< bluetoe::link_layer::connectable_undirected_advertising >();
    }
    else
    {
        link_layer.change_advertising< bluetoe::link_layer::connectable_directed_advertising >();
        link_layer.directed_advertising_address(
            bluetoe::link_layer::random_device_address( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } ) );
    }

    return bluetoe::error_codes::success;
}
