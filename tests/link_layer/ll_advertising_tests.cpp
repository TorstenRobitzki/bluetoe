#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "../test_servers.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <map>

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
    BOOST_CHECK_GT( transmitted_data().size(), 0 );
}

BOOST_FIXTURE_TEST_CASE( advertising_uses_all_three_adv_channels, advertising )
{
    std::map< unsigned, unsigned > channel;

    all_data( [&]( const test::scheduled_data& d ) { ++channel[ d.channel ]; } );

    BOOST_CHECK_EQUAL( channel.size(), 3u );
    BOOST_CHECK_GT( channel[ 37 ], 0 );
    BOOST_CHECK_GT( channel[ 38 ], 0 );
    BOOST_CHECK_GT( channel[ 39 ], 0 );
}

/**
 * @test by default, the link layer will advertise Connectable Undirected Events
 */
BOOST_FIXTURE_TEST_CASE( connectable_undirected_is_the_default, advertising )
{
}