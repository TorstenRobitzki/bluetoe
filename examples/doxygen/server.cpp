
#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <cstdint>

std::int32_t temperature;

typedef bluetoe::server<
    /*
     * A server consists of at least one service
     */
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
            bluetoe::no_read_access
        >
    >
> temperature_service;
