#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "../test_servers.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {

    struct advertising : bluetoe::link_layer::link_layer< small_temperature_service, test::radio >
    {
        advertising()
        {
            this->run( gatt_server_ );
        }

        small_temperature_service gatt_server_;
    };

}

BOOST_FIXTURE_TEST_CASE( advertising_scheduled, advertising )
{
    BOOST_CHECK_EQUAL( transmitted_data().size(), 1u );
}
