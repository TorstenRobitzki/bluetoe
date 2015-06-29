#include <iostream>
#include <map>

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "../test_servers.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {

    template < typename ... Options >
    struct advertising_base : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... >
    {
        advertising_base()
        {
            this->run( gatt_server_ );
        }

        small_temperature_service gatt_server_;
    };

    struct advertising : advertising_base<> {};

    static const auto filter_channel_37 = []( const test::schedule_data& a )
    {
        return a.channel == 37;
    };

}

BOOST_FIXTURE_TEST_CASE( advertising_scheduled, advertising )
{
    BOOST_CHECK_GT( scheduling().size(), 0 );
}

BOOST_FIXTURE_TEST_CASE( advertising_uses_all_three_adv_channels, advertising )
{
    std::map< unsigned, unsigned > channel;

    all_data( [&]( const test::schedule_data& d ) { ++channel[ d.channel ]; } );

    BOOST_CHECK_EQUAL( channel.size(), 3u );
    BOOST_CHECK_GT( channel[ 37 ], 0 );
    BOOST_CHECK_GT( channel[ 38 ], 0 );
    BOOST_CHECK_GT( channel[ 39 ], 0 );
}

BOOST_FIXTURE_TEST_CASE( channels_are_iterated, advertising )
{
    check_scheduling(
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            return a.channel != b.channel;
        },
        "channels_are_iterated"
    );
}

/**
 * @test by default, the link layer will advertise Connectable Undirected Events
 */
BOOST_FIXTURE_TEST_CASE( connectable_undirected_is_the_default, advertising )
{
    check_scheduling(
        [&]( const test::schedule_data& scheduled ) -> bool
        {
            return !scheduled.transmitted_data.empty()
                && ( scheduled.transmitted_data[ 0 ] & 0xF0 ) == 0;
        },
        "channels_are_iterated"
    );
}

/**
 * @test according to the specs, the distance between two advertising PDUs shall be short than or equal to 10ms
 *       We identify the start of an advertising event by the first channel 37
 */
BOOST_FIXTURE_TEST_CASE( less_than_10ms_between_two_PDUs, advertising )
{
    check_scheduling(
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            return a.channel != 37 && a.channel != 38
                || ( b.on_air_time - a.on_air_time ) <= bluetoe::link_layer::delta_time::msec( 10 );
        },
        "less_than_10ms_between_two_PDUs"
    );
}

/**
 * @test the advertising interval is 50ms plus a pseudo random interval of 0ms to 10ms
 */
BOOST_FIXTURE_TEST_CASE( configured_advertising_interval_is_kept, advertising_base< bluetoe::link_layer::advertising_interval< 50 > > )
{
    check_scheduling(
        filter_channel_37,
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 50 )
                && diff <= bluetoe::link_layer::delta_time::msec( 60 );
        },
        "configured_advertising_interval_is_kept"
    );
}

/**
 * @test by default, the interval is 100ms
 */
BOOST_FIXTURE_TEST_CASE( default_advertising_interval_is_kept, advertising )
{
    check_scheduling(
        filter_channel_37,
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 100 )
                && diff <= bluetoe::link_layer::delta_time::msec( 110 );
        },
        "configured_advertising_interval_is_kept"
    );
}

/**
 * @test perturbation looks quit random
 * - no two adjanced equal values
 * - all values are equally distributed.
 */
BOOST_FIXTURE_TEST_CASE( perturbation_looks_quit_random, advertising )
{
    std::map< bluetoe::link_layer::delta_time, unsigned >   perturbations;
    bluetoe::link_layer::delta_time                         last_perturbation = bluetoe::link_layer::delta_time::now();

    all_data(
        filter_channel_37,
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            const auto current_perturbation = b.on_air_time - a.on_air_time - bluetoe::link_layer::delta_time::msec( 100 );

            if ( !perturbations.empty() )
            {
                BOOST_CHECK_NE( last_perturbation, current_perturbation );
            }

            ++perturbations[ current_perturbation ];
            last_perturbation = current_perturbation;
        }
    );

    double average = 0.0;
    unsigned count = 0;

    for ( auto p : perturbations )
    {
        average += p.first.usec() * p.second;
        count   += p.second;
    }

    BOOST_CHECK_CLOSE_FRACTION( ( average / count ), 5.0 * 1000, 0.2 );
}
