#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/link_layer.hpp>
#include "transport.hpp"

std::uint16_t value = 0x0815;

using simple_gatt_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x4766 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x2021 >,
            bluetoe::bind_characteristic_value< std::uint16_t, &value >
        >
    >
>;

using link_layer = bluetoe::hci::link_layer< simple_gatt_server, test::transport >;

BOOST_FIXTURE_TEST_CASE( starts_advertising, link_layer )
{

}