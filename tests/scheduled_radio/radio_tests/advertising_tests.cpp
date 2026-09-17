/**
 * @file advertising_tests.cpp
 *
 * start_advertising() and schedule_advertising_event() over the air, observed by the tester.
 * Needs the tester; skipped without one.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/dut.hpp"
#include "test_tools/observations.hpp"
#include "test_tools/records.hpp"
#include "test_tools/rig_fixture.hpp"
#include "test_tools/tester.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    // how long a tester operation may wait for what it waits for
    const time_out operation_timeout{ 300ms };

    // a scanner the device's acceptance filter does not hold: the tester's address, one byte off
    const bluetoe::link_layer::device_address stranger_address{ { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02 }, false };

    // advertisers a scan request can be addressed to instead of the device: other bytes, and
    // the device's bytes as a random address
    const bluetoe::link_layer::device_address other_advertiser{ { 0x11, 0x22, 0x33, 0x44, 0x55, 0xc1 }, false };
    const bluetoe::link_layer::device_address dut_bytes_as_random{ { 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0 }, true };

    // the address the device changes to, a static random one
    const bluetoe::link_layer::device_address changed_address{ { 0x66, 0x55, 0x44, 0x33, 0x22, 0xd1 }, true };

    constexpr std::uint32_t other_access_address = 0x71764129;
    constexpr std::uint32_t other_crc_init       = 0x7a8f23;

    /*
     * One advertising started on `first_channel`, the next scheduled an interval later on
     * `second_channel`. The tester listens on the first channel, then on the second, so each
     * is seen only if it went out on its channel. At the smallest payload the two PDUs are
     * the same bytes, since six bytes are the advertiser's address alone.
     */
    void two_advertisings_are_seen_as_sent(
        rig_fixture& rig, std::size_t payload_size, std::uint32_t first_channel, std::uint32_t second_channel )
    {
        constexpr auto interval = 100ms;
        const auto     first    = advertising( payload_size, 0x01 );
        const auto     second   = advertising( payload_size, 0x02 );

        rig.program_device( {
            on_start(       start_advertising(          first_channel,              first ) ),
            on_adv_timeout( schedule_advertising_event( second_channel, interval,   second ) ) } );

        // the first goes out within milliseconds of the start, the second at the interval
        rig.program_tester( {
            listen( first_channel,  50ms ),
            listen( second_channel, 250ms ) } );

        rig.run();

        const auto captured = rig.check_captured( {
            received( first ),
            received( second ) } );

        BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 0 ], captured[ 1 ] ) - interval ), tolerance );
    }

    // a scan request with its length field set to `length`, its payload cut or padded to match
    std::vector< std::uint8_t > with_length( std::span< const std::uint8_t > request, std::uint8_t length )
    {
        std::vector< std::uint8_t > result( request.begin(), request.end() );
        result.resize( 2 + length, 0 );
        result[ 1 ] = length;

        return result;
    }

    /*
     * The time from the end of `earlier` to the first bit of `later`: preamble, access
     * address, the PDU and its CRC at 1 Mbit are 8 µs a byte.
     */
    tester_duration inter_frame_space( const captured_pdu& earlier, const captured_pdu& later )
    {
        const std::chrono::microseconds air_time( ( 1 + 4 + earlier.data.size + 3 ) * 8 );

        return time_of( later.when ) - time_of( earlier.when ) - air_time;
    }

    /*
     * The tester answers the first advertising, of `type`, with `request`: the device answers
     * with its scan response and reports the request with adv_received(). Returns what the
     * tester captured: the advertising, the request and the response.
     */
    std::vector< captured_pdu > a_scan_request_is_answered(
        rig_fixture_without_device_filter& rig, std::span< const std::uint8_t > request, std::uint8_t type = adv_scan_ind )
    {
        const auto advertisement = advertising( 6, 0x01, type );
        const auto response      = advertising( 10, 0x02, scan_rsp );

        rig.program_device( {
            on_start(
                set_local_address( dut_address ),
                start_advertising( 37, advertisement, response ) ) } );

        rig.program_tester( {
            answer( 37, dut_address, request, time_out( 100ms ) ) } );

        rig.run();

        const auto captured = rig.check_captured( {
            received( advertisement ),
            sent( request ),
            received( response ) } );

        BOOST_CHECK( the_only( callbacks_of( rig.device_records(), adv_received ) ).data == pdu( request ) );

        return captured;
    }

    /*
     * The tester answers the first advertising, of `type` with a payload of `payload_size`, with
     * `request`, which the device does not answer; the event ends with adv_timeout(), and the
     * advertising scheduled from that shows the device went on.
     */
    void a_scan_request_is_ignored(
        rig_fixture_without_device_filter& rig, std::span< const std::uint8_t > request,
        std::uint8_t type = adv_scan_ind, std::size_t payload_size = 7 )
    {
        const auto first    = advertising( payload_size, 0x01, type );
        const auto next     = advertising( payload_size, 0x02, type );
        const auto response = advertising( 10, 0x03, scan_rsp );

        rig.program_device( {
            on_start(
                set_local_address( dut_address ),
                start_advertising( 37, first, response ) ),
            on_adv_timeout(
                schedule_advertising_event( 37, 100ms, next, response ) ) } );

        // the answer gets no reply and runs to its window, which closes before the next advertising
        rig.program_tester( {
            answer( 37, dut_address, request, time_out( 50ms ) ),
            receive( 37, 1, operation_timeout ) } );

        rig.run();

        rig.check_captured( {
            received( first ),
            sent( request ),
            received( next ) } );

        check_callbacks( rig.device_records(), { adv_timeout, adv_timeout } );
    }

    // the device moves off the advertising access address before it starts
    std::vector< std::uint8_t > advertise_on_another_access_address( rig_fixture& rig )
    {
        const auto advertisement = advertising( 6, 0 );

        rig.program_device( {
            on_start(
                set_access_address_and_crc_init( other_access_address, other_crc_init ),
                start_advertising( 37, advertisement ) ),
            on_adv_timeout( schedule_advertising_event( 37, 100ms, advertisement ) ) } );
        rig.program_tester( { listen( 37, 300ms ) } );

        return advertisement;
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
        on_adv_timeout( schedule_advertising_event( 37, 100ms, advertisement ) ),
        on_adv_timeout( schedule_advertising_event( 37, 50ms,  advertisement ) ),
        on_adv_timeout( schedule_advertising_event( 37, 150ms, advertisement ) ) } );
    program_tester( { listen( 37, 500ms ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        received( advertisement ),
        received( advertisement ),
        received( advertisement ) } );

    const std::chrono::milliseconds requested[] = { 100ms, 50ms, 150ms };

    for ( std::size_t i = 0; i != std::size( requested ); ++i )
        BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ i ], captured[ i + 1 ] ) - requested[ i ] ), tolerance );
}

/*
 * The tester follows the device from channel to channel: each receive ends with the one
 * advertising it waits for, so the next listens on the channel the device moves to.
 */
BOOST_FIXTURE_TEST_CASE( an_advertiser_can_be_followed_over_all_channels, rig_fixture, *if_tester )
{
    const std::uint32_t channels[] = { 37, 38, 39, 37 };
    constexpr auto      interval   = 100ms;

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
        receive( channels[ 0 ], 1, operation_timeout ),
        receive( channels[ 1 ], 1, operation_timeout ),
        receive( channels[ 2 ], 1, operation_timeout ),
        receive( channels[ 3 ], 1, operation_timeout ) } );

    run();

    std::vector< expected_pdu > expected;

    for ( const auto& advertisement : sent )
        expected.push_back( received( advertisement ) );

    const auto captured = check_captured( expected );

    for ( std::size_t i = 1; i != captured.size(); ++i )
        BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ i - 1 ], captured[ i ] ) - interval ), tolerance );
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
        on_adv_timeout( schedule_advertising_event( 37, 100ms, scheduled ) ) } );
    program_tester( { listen( 37, 400ms ) } );

    run();

    const auto captured = check_captured( {
        received( first ),
        received( restarted ),
        received( scheduled ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 1 ], captured[ 2 ] ) - 100ms ), tolerance );
}

BOOST_FIXTURE_TEST_CASE( advertising_can_be_started_again_after_scheduled_events, rig_fixture, *if_tester )
{
    std::vector< std::vector< std::uint8_t > > sent;

    for ( std::uint8_t fill = 0; fill != 5; ++fill )
        sent.push_back( advertising( 7, fill ) );

    program_device( {
        on_start(       start_advertising(          37,                         sent[ 0 ] ) ),
        on_adv_timeout( schedule_advertising_event( 37, 100ms, sent[ 1 ] ) ),
        on_adv_timeout( schedule_advertising_event( 37, 100ms, sent[ 2 ] ) ),
        on_adv_timeout( start_advertising(          37,                         sent[ 3 ] ) ),
        on_adv_timeout( schedule_advertising_event( 37, 100ms, sent[ 4 ] ) ) } );
    program_tester( { listen( 37, 600ms ) } );

    run();

    std::vector< expected_pdu > expected;

    for ( const auto& advertisement : sent )
        expected.push_back( received( advertisement ) );

    const auto captured = check_captured( expected );

    // placed from the start before them; the restart itself has no required time
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 0 ], captured[ 1 ] ) - 100ms ), tolerance );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 1 ], captured[ 2 ] ) - 100ms ), tolerance );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 3 ], captured[ 4 ] ) - 100ms ), tolerance );
}

// a tester that follows the device to its access address hears both with a valid CRC
BOOST_FIXTURE_TEST_CASE( advertising_uses_the_access_address_and_crc_init_that_were_set, rig_fixture, *if_tester )
{
    observer.call< &tester::set_access_address_and_crc_init >( other_access_address, other_crc_init );

    const auto advertisement = advertise_on_another_access_address( *this );

    run();

    check_captured( {
        received( advertisement ),
        received( advertisement ) } );
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
    a_scan_request_is_answered( *this, scan_request( tester_address, dut_address ) );
}

/*
 * The tester lets the first advertising pass and answers the scheduled one: the device
 * answers a request to an event placed by schedule_advertising_event() as well. The last
 * advertising is scheduled from adv_received(), so seeing it shows that the request was
 * reported, and at the time its first bit was on air.
 */
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_a_scheduled_advertising_is_answered, rig_fixture, *if_tester )
{
    const auto first     = advertising( 7, 0x01, adv_scan_ind );
    const auto scheduled = advertising( 7, 0x02, adv_scan_ind );
    const auto last      = advertising( 7, 0x04, adv_scan_ind );
    const auto response  = advertising( 10, 0x03, scan_rsp );
    const auto request   = scan_request( tester_address, dut_address );

    program_device( {
        on_start(
            set_local_address( dut_address ),
            start_advertising( 37, first, response ) ),
        on_adv_timeout(
            schedule_advertising_event( 37, 100ms, scheduled, response ) ),
        on_adv_received(
            schedule_advertising_event( 37, 100ms, last, response ) ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        answer( 37, dut_address, request, operation_timeout ),
        receive( 37, 1, operation_timeout ) } );

    run();

    const auto captured = check_captured( {
        received( first ),
        received( scheduled ),
        sent( request ),
        received( response ),
        received( last ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 2 ], captured[ 4 ] ) - 100ms ), tolerance );

    const auto records = device_records();

    check_callbacks( records, { adv_timeout, adv_received, adv_timeout } );

    BOOST_CHECK( the_only( callbacks_of( records, adv_received ) ).data == pdu( request ) );
}

/*
 * A scan request from a scanner outside the acceptance filter is not answered, and the event
 * ends with adv_timeout().
 */
BOOST_FIXTURE_TEST_CASE( a_scan_request_from_outside_the_acceptance_filter_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, scan_request( stranger_address, dut_address ) );
}

// the same with the tester's address, from a filter that holds another scanner only
BOOST_FIXTURE_TEST_CASE( a_scan_request_is_ignored_when_the_filter_holds_another_scanner, rig_fixture_without_device_filter, *if_tester )
{
    BOOST_REQUIRE( device.call< &dut::add_to_acceptance_filter >( stranger_address ) );

    a_scan_request_is_ignored( *this, scan_request( tester_address, dut_address ) );
}

// an empty acceptance filter accepts every scanner (scheduled_radio2.hpp)
BOOST_FIXTURE_TEST_CASE( a_scan_request_from_any_scanner_is_answered_with_an_empty_filter, rig_fixture_without_device_filter, *if_tester )
{
    a_scan_request_is_answered( *this, scan_request( stranger_address, dut_address ) );
}

// the device answers only requests addressed to itself: the address's bytes
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_another_advertiser_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, scan_request( tester_address, other_advertiser ) );
}

// and its kind, public or random, by the request's RxAdd bit
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_the_device_address_as_random_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, scan_request( tester_address, dut_bytes_as_random ) );
}

/*
 * set_local_address() between two events changes the address a request has to be addressed
 * to: the device answers a request to its first address, then, after the change, ignores a
 * request to the old one and answers one to the new one.
 */
BOOST_FIXTURE_TEST_CASE( a_changed_local_address_is_respected, rig_fixture, *if_tester )
{
    const auto old_advertising   = advertising( 7, 0x01, adv_scan_ind );
    const auto old_response      = advertising( 10, 0x02, scan_rsp );
    const auto new_advertising   = advertising( 7, 0x03, adv_scan_ind, changed_address );
    const auto next_advertising  = advertising( 7, 0x04, adv_scan_ind, changed_address );
    const auto new_response      = advertising( 10, 0x05, scan_rsp, changed_address );
    const auto request_to_old    = scan_request( tester_address, dut_address );
    const auto request_to_new    = scan_request( tester_address, changed_address );

    // the tester reports the device under its new address as well
    BOOST_REQUIRE( observer.call< &tester::add_to_acceptance_filter >( changed_address ) );

    program_device( {
        on_start(
            set_local_address( dut_address ),
            start_advertising( 37, old_advertising, old_response ) ),
        on_adv_received(
            set_local_address( changed_address ),
            schedule_advertising_event( 37, 100ms, new_advertising, new_response ) ),
        on_adv_timeout(
            schedule_advertising_event( 37, 100ms, next_advertising, new_response ) ) } );

    // the request to the old address gets no reply; its window closes before the next advertising
    program_tester( {
        answer( 37, dut_address, request_to_old, operation_timeout ),
        answer( 37, changed_address, request_to_old, time_out( 150ms ) ),
        answer( 37, changed_address, request_to_new, operation_timeout ) } );

    run();

    check_captured( {
        received( old_advertising ),
        sent( request_to_old ),
        received( old_response ),
        received( new_advertising ),
        sent( request_to_old ),
        received( next_advertising ),
        sent( request_to_new ),
        received( new_response ) } );

    check_callbacks( device_records(), { adv_received, adv_timeout, adv_received } );
}

/*
 * The scan response starts one inter frame space, 150 µs, after the request ended. The
 * tolerance covers the tester's placement of a received packet; the tester's offset for that
 * was measured against this very response, so this guards against a device that answers
 * off T_IFS, not the calibration.
 */
BOOST_FIXTURE_TEST_CASE( the_scan_response_starts_one_inter_frame_space_after_the_request, rig_fixture, *if_tester )
{
    const auto   captured = a_scan_request_is_answered( *this, scan_request( tester_address, dut_address ) );
    const auto   space    = inter_frame_space( captured[ 1 ], captured[ 2 ] );
    const double space_us = std::chrono::duration< double, std::micro >( space ).count();

    BOOST_CHECK_MESSAGE( std::chrono::abs( space - 150us ) <= 2us, "inter frame space " << space_us << " µs" );
}

// a scan request has a payload of 12 bytes, the two addresses, and nothing else is one
BOOST_FIXTURE_TEST_CASE( a_scan_request_that_is_too_short_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, with_length( scan_request( tester_address, dut_address ), 11 ) );
}

BOOST_FIXTURE_TEST_CASE( a_scan_request_that_is_too_long_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, with_length( scan_request( tester_address, dut_address ), 13 ) );
}

// the same two addresses under another PDU type, ADV_IND, are not a request
BOOST_FIXTURE_TEST_CASE( a_pdu_of_another_type_is_not_answered, rig_fixture, *if_tester )
{
    constexpr std::uint8_t pdu_type_mask = 0x0f;

    auto request = scan_request( tester_address, dut_address );
    request[ 0 ] = ( request[ 0 ] & ~pdu_type_mask ) | adv_ind;

    a_scan_request_is_ignored( *this, request );
}

// ADV_IND can be scanned as well as ADV_SCAN_IND
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_an_adv_ind_is_answered, rig_fixture, *if_tester )
{
    a_scan_request_is_answered( *this, scan_request( tester_address, dut_address ), adv_ind );
}

// an advertising that can not be scanned is not answered, though the event has a response
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_an_adv_nonconn_ind_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, scan_request( tester_address, dut_address ), adv_nonconn_ind );
}

// ADV_DIRECT_IND carries the device's address and a target address, 12 bytes
BOOST_FIXTURE_TEST_CASE( a_scan_request_to_an_adv_direct_ind_is_ignored, rig_fixture, *if_tester )
{
    a_scan_request_is_ignored( *this, scan_request( tester_address, dut_address ), adv_direct_ind, 12 );
}
