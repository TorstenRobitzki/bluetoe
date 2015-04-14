#ifndef BLUETOE_TESTS_TEST_SERVICES_HPP
#define BLUETOE_TESTS_TEST_SERVICES_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

namespace {
    std::uint32_t global_temperature;

    typedef bluetoe::service<
        bluetoe::bind_characteristic_value< std::uint32_t, &global_temperature >,
        bluetoe::service_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
    > global_temperature_service;

    typedef bluetoe::service<
        bluetoe::service_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
    > empty_service;

    bool                characteristic_value_1 = 1;
    const std::uint64_t characteristic_value_2 = 2;
    const std::int16_t  characteristic_value_3 = 3;

    typedef bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( characteristic_value_1 ), &characteristic_value_1 >,
            bluetoe::no_read_access
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAB >,
            bluetoe::bind_characteristic_value< decltype( characteristic_value_2 ), &characteristic_value_2 >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x0815 >,
            bluetoe::bind_characteristic_value< decltype( characteristic_value_3 ), &characteristic_value_3 >
        >
    > service_with_3_characteristics;
}

#endif
