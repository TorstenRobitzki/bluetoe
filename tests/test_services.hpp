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
}

#endif
