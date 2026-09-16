/**
 * @file advertising_tests.cpp
 *
 * start_advertising() and schedule_advertising_event() over the air, observed by the tester.
 * Needs the tester; skipped without one.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "radio_tests/dut.hpp"
#include "radio_tests/rig_fixture.hpp"
#include "radio_tests/tester.hpp"

#include <bluetoe/delta_time.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;
using bluetoe::link_layer::delta_time;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    // how far an observed interval may be off the requested one: the placement of both
    // events and the drift of two stock crystals over an interval
    constexpr long tolerance_us = 50;

    constexpr std::uint32_t other_access_address = 0x71764129;
    constexpr std::uint32_t other_crc_init       = 0x7a8f23;

    // an ADV_NONCONN_IND of `payload_size` bytes: the device's address, then `fill`
    std::vector< std::uint8_t > advertising( std::size_t payload_size, std::uint8_t fill )
    {
        std::vector< std::uint8_t > result( 2 + payload_size, fill );
        result[ 0 ] = 0x02;
        result[ 1 ] = static_cast< std::uint8_t >( payload_size );
        std::copy( dut_address.begin(), dut_address.end(), result.begin() + 2 );

        return result;
    }

    bool carries( const captured_pdu& p, std::span< const std::uint8_t > bytes )
    {
        return p.data.size == bytes.size()
            && std::equal( bytes.begin(), bytes.end(), p.data.data.begin() );
    }

    long microseconds_between( const captured_pdu& earlier, const captured_pdu& later )
    {
        return std::chrono::duration_cast< std::chrono::microseconds >(
            time_of( later.when ) - time_of( earlier.when ) ).count();
    }

    /*
     * One advertising started on `first_channel`, the next scheduled an interval later on
     * `second_channel`. The tester listens on the first channel, then on the second, so each
     * is seen only if it went out on its channel.
     */
    void two_advertisings_are_seen_as_sent(
        rig_fixture& rig, std::size_t payload_size, std::uint32_t first_channel, std::uint32_t second_channel )
    {
        const delta_time interval = delta_time::msec( 100 );
        const auto       first    = advertising( payload_size, 0x01 );
        const auto       second   = advertising( payload_size, 0x02 );

        rig.program_device( {
            on_start(       start_advertising(          first_channel,              first ) ),
            on_adv_timeout( schedule_advertising_event( second_channel, interval,   second ) ) } );

        // the first goes out within milliseconds of the start, the second at the interval
        rig.program_tester( {
            receive( first_channel,  delta_time::msec( 50 ) ),
            receive( second_channel, delta_time::msec( 250 ) ) } );

        rig.run();

        const auto captured = rig.tester_captured();

        BOOST_REQUIRE_EQUAL( captured.size(), 2u );
        BOOST_CHECK( carries( captured[ 0 ], first ) );
        BOOST_CHECK( carries( captured[ 1 ], second ) );
        BOOST_CHECK_LE( std::abs( microseconds_between( captured[ 0 ], captured[ 1 ] ) - interval.usec() ), tolerance_us );
    }

    // the device moves off the advertising access address before it starts
    void advertise_on_another_access_address( rig_fixture& rig )
    {
        const auto advertisement = advertising( 6, 0 );

        rig.program_device( {
            on_start(
                set_access_address_and_crc_init( other_access_address, other_crc_init ),
                start_advertising( 37, advertisement ) ),
            on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), advertisement ) ) } );
        rig.program_tester( { receive( 37, delta_time::msec( 300 ) ) } );
    }
}

BOOST_FIXTURE_TEST_CASE( the_smallest_advertising_is_sent_on_the_requested_channels, rig_fixture, *if_tester )
{
    two_advertisings_are_seen_as_sent( *this, 6, 37, 38 );
}

BOOST_FIXTURE_TEST_CASE( a_middle_sized_advertising_is_sent_on_the_requested_channels, rig_fixture, *if_tester )
{
    two_advertisings_are_seen_as_sent( *this, 20, 38, 39 );
}

BOOST_FIXTURE_TEST_CASE( the_largest_advertising_is_sent_on_the_requested_channels, rig_fixture, *if_tester )
{
    two_advertisings_are_seen_as_sent( *this, 37, 39, 37 );
}

BOOST_FIXTURE_TEST_CASE( the_interval_can_change_while_advertising, rig_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0 );

    program_device( {
        on_start(       start_advertising(          37,                         advertisement ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), advertisement ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 50 ),  advertisement ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 150 ), advertisement ) ) } );
    program_tester( { receive( 37, delta_time::msec( 500 ) ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 4u );

    const long requested_us[] = { 100'000, 50'000, 150'000 };

    for ( std::size_t i = 0; i != std::size( requested_us ); ++i )
        BOOST_CHECK_LE( std::abs( microseconds_between( captured[ i ], captured[ i + 1 ] ) - requested_us[ i ] ), tolerance_us );
}

// a tester that follows the device to its access address hears both with a valid CRC
BOOST_FIXTURE_TEST_CASE( advertising_uses_the_access_address_and_crc_init_that_were_set, rig_fixture, *if_tester )
{
    observer.call< &tester::set_access_address_and_crc_init >( other_access_address, other_crc_init );
    advertise_on_another_access_address( *this );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 2u );
    BOOST_CHECK( captured[ 0 ].crc_ok );
    BOOST_CHECK( captured[ 1 ].crc_ok );
}

// one that stays on the advertising access address hears nothing
BOOST_FIXTURE_TEST_CASE( advertising_on_another_access_address_is_not_heard_on_the_advertising_one, rig_fixture, *if_tester )
{
    advertise_on_another_access_address( *this );

    run();

    BOOST_CHECK( tester_captured().empty() );
}
