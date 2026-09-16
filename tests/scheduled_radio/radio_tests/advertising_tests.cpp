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

    constexpr std::uint8_t adv_nonconn_ind = 0x02;
    constexpr std::uint8_t scan_rsp        = 0x04;
    constexpr std::uint8_t adv_scan_ind    = 0x06;

    // an advertising channel PDU of `payload_size` bytes: the device's address, then `fill`
    std::vector< std::uint8_t > advertising( std::size_t payload_size, std::uint8_t fill, std::uint8_t type = adv_nonconn_ind )
    {
        std::vector< std::uint8_t > result( 2 + payload_size, fill );
        result[ 0 ] = type;
        result[ 1 ] = static_cast< std::uint8_t >( payload_size );
        std::copy( dut_address.begin(), dut_address.end(), result.begin() + 2 );

        return result;
    }

    std::vector< record > callbacks_of( const std::vector< record >& records, callback_kind kind )
    {
        std::vector< record > result;
        std::copy_if( records.begin(), records.end(), std::back_inserter( result ),
            [ kind ]( const record& r ){ return r.kind == record_kind::callback && r.callback == kind; } );

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

/*
 * The tester follows the device from channel to channel: each receive ends with the one
 * advertising it waits for, so the next listens on the channel the device moves to.
 */
BOOST_FIXTURE_TEST_CASE( an_advertiser_can_be_followed_over_all_channels, rig_fixture, *if_tester )
{
    const std::uint32_t channels[] = { 37, 38, 39, 37 };
    const delta_time    interval   = delta_time::msec( 100 );

    std::vector< std::vector< std::uint8_t > > sent;

    for ( std::uint8_t fill = 0; fill != std::size( channels ); ++fill )
        sent.push_back( advertising( 7, fill ) );

    program_device( {
        on_start(       start_advertising(          channels[ 0 ],           sent[ 0 ] ) ),
        on_adv_timeout( schedule_advertising_event( channels[ 1 ], interval, sent[ 1 ] ) ),
        on_adv_timeout( schedule_advertising_event( channels[ 2 ], interval, sent[ 2 ] ) ),
        on_adv_timeout( schedule_advertising_event( channels[ 3 ], interval, sent[ 3 ] ) ) } );

    // the window only bounds a lost advertising
    program_tester( {
        receive( channels[ 0 ], delta_time::msec( 300 ), 1 ),
        receive( channels[ 1 ], delta_time::msec( 300 ), 1 ),
        receive( channels[ 2 ], delta_time::msec( 300 ), 1 ),
        receive( channels[ 3 ], delta_time::msec( 300 ), 1 ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), sent.size() );

    for ( std::size_t i = 0; i != sent.size(); ++i )
        BOOST_CHECK( carries( captured[ i ], sent[ i ] ) );

    for ( std::size_t i = 1; i != captured.size(); ++i )
        BOOST_CHECK_LE( std::abs( microseconds_between( captured[ i - 1 ], captured[ i ] ) - interval.usec() ), tolerance_us );
}

/*
 * start_advertising() from the callback of an event restarts the sequence. The restart has no
 * required time, so only what follows it is measured: it is placed from the restart.
 */
BOOST_FIXTURE_TEST_CASE( advertising_can_be_started_again_from_the_callback_of_the_first, rig_fixture, *if_tester )
{
    const auto first     = advertising( 7, 1 );
    const auto restarted = advertising( 7, 2 );
    const auto scheduled = advertising( 7, 3 );

    program_device( {
        on_start(       start_advertising(          37,                         first ) ),
        on_adv_timeout( start_advertising(          37,                         restarted ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), scheduled ) ) } );
    program_tester( { receive( 37, delta_time::msec( 400 ) ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 3u );
    BOOST_CHECK( carries( captured[ 0 ], first ) );
    BOOST_CHECK( carries( captured[ 1 ], restarted ) );
    BOOST_CHECK( carries( captured[ 2 ], scheduled ) );
    BOOST_CHECK_LE( std::abs( microseconds_between( captured[ 1 ], captured[ 2 ] ) - 100'000 ), tolerance_us );
}

BOOST_FIXTURE_TEST_CASE( advertising_can_be_started_again_after_scheduled_events, rig_fixture, *if_tester )
{
    std::vector< std::vector< std::uint8_t > > sent;

    for ( std::uint8_t fill = 0; fill != 5; ++fill )
        sent.push_back( advertising( 7, fill ) );

    program_device( {
        on_start(       start_advertising(          37,                         sent[ 0 ] ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), sent[ 1 ] ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), sent[ 2 ] ) ),
        on_adv_timeout( start_advertising(          37,                         sent[ 3 ] ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), sent[ 4 ] ) ) } );
    program_tester( { receive( 37, delta_time::msec( 600 ) ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), sent.size() );

    for ( std::size_t i = 0; i != sent.size(); ++i )
        BOOST_CHECK( carries( captured[ i ], sent[ i ] ) );

    // placed from the start before them; the restart itself has no required time
    BOOST_CHECK_LE( std::abs( microseconds_between( captured[ 0 ], captured[ 1 ] ) - 100'000 ), tolerance_us );
    BOOST_CHECK_LE( std::abs( microseconds_between( captured[ 1 ], captured[ 2 ] ) - 100'000 ), tolerance_us );
    BOOST_CHECK_LE( std::abs( microseconds_between( captured[ 3 ], captured[ 4 ] ) - 100'000 ), tolerance_us );
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

/*
 * The tester answers the first advertising with a scan request from its own address: the
 * device answers that with its scan response and reports the request with adv_received().
 */
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_the_first_advertising_is_answered, rig_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01, adv_scan_ind );
    const auto response      = advertising( 10, 0x02, scan_rsp );
    const auto request       = scan_request( tester_address, dut_address );

    program_device( {
        on_start(
            set_local_address( dut_address ),
            start_advertising( 37, advertisement, response ) ) } );

    program_tester( {
        answer( 37, delta_time::msec( 100 ), dut_address, request ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 3u );
    BOOST_CHECK( captured[ 0 ].direction == pdu_direction::received );
    BOOST_CHECK( carries( captured[ 0 ], advertisement ) );
    BOOST_CHECK( captured[ 1 ].direction == pdu_direction::transmitted );
    BOOST_CHECK( carries( captured[ 1 ], request ) );
    BOOST_CHECK( captured[ 2 ].direction == pdu_direction::received );
    BOOST_CHECK( captured[ 2 ].crc_ok );
    BOOST_CHECK( carries( captured[ 2 ], response ) );

    const auto received = callbacks_of( device_records(), callback_kind::adv_received );

    BOOST_REQUIRE_EQUAL( received.size(), 1u );
    BOOST_CHECK( received[ 0 ].data == pdu( request ) );
}
