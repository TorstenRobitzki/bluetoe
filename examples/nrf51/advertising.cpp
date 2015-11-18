#include <bluetoe/bindings/nrf51.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/server.hpp>

std::uint32_t temperature_value = 0x12345678;
static constexpr char server_name[] = "Temperature";
static constexpr char char_name[] = "Temperature Value";

typedef bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::no_write_access,
            bluetoe::characteristic_name< char_name >,
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >
        >
    >
> small_temperature_service;

int main()
{
    small_temperature_service                   gatt;
    bluetoe::nrf51<
        small_temperature_service,
        bluetoe::link_layer::advertising_interval< 250u >,
        bluetoe::link_layer::static_address< 0xf0, 0xa6, 0xd5, 0x4f, 0x60, 0x0b >
    > server;

    for ( ;; )
        server.run( gatt );
}
