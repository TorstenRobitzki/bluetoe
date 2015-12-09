#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <cstdint>

std::int32_t temperature;

using sensor_position_uuid = bluetoe::service_uuid< 0xD9473E00, 0xE7D3, 0x4D90, 0x9366, 0x282AC4F44FEB >;

typedef bluetoe::server<
    /*
     * A server consists of two services, on beeing included to the other
     */
    bluetoe::secondary_service<
        sensor_position_uuid,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::include_service< sensor_position_uuid >,
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
            bluetoe::no_read_access
        >
    >
> temperature_service;
