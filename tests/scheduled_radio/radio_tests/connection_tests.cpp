/**
 * @file connection_tests.cpp
 *
 * schedule_connection_event() over the air. The device advertises first, as every test does,
 * and schedules the connection event from that advertising; the tester places the first PDU
 * of the event from the same advertising, as the central of the connection. Needs the tester;
 * skipped without one.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/dut.hpp"
#include "test_tools/observations.hpp"
#include "test_tools/records.hpp"
#include "test_tools/rig_fixture.hpp"
#include "test_tools/tester.hpp"

#include "host/central.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    // how long a tester operation may wait for what it waits for
    const time_out operation_timeout{ 300ms };

    constexpr std::uint32_t connection_access_address = 0x71764129;
    constexpr std::uint32_t connection_crc_init       = 0x7a8f23;
    constexpr std::uint32_t data_channel              = 5;

    // an access address of another connection
    constexpr std::uint32_t other_access_address      = 0x2f6c9d31;

    // the connection event starts this long after the advertising, and receives until
    // `receive_window` later without a reception
    constexpr auto event_start    = 50ms;
    constexpr auto receive_window = 2ms;

    // the tester's first PDU of the event begins this long after the advertising, 500 µs into
    // the receive window
    constexpr auto first_pdu_after_advertising = event_start + 500us;

    const std::uint8_t some_data[] = { 0x01, 0x02, 0x03 };


    /*
     * The central's first PDU has its first bit on air `at` after the advertising, which the
     * device's event is placed from as well: the device answers it, and the event ends with its
     * anchor there.
     */
    void a_pdu_is_received( connection_fixture& rig, std::chrono::microseconds at )
    {
        const auto advertisement = advertising( 6, 0x01 );

        central tester_side;
        const auto first = tester_side.send();

        rig.program_device( {
            on_start(
                start_advertising( 37, advertisement ) ),
            on_adv_timeout(
                set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
                schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
            on_connection_end_event() } );

        rig.program_tester( {
            receive( 37, 1, operation_timeout ),
            use_access_address( connection_access_address, connection_crc_init ),
            connection_event( data_channel, at, { first } ) } );

        rig.run();

        rig.check_captured( {
            received( advertisement ),
            sent( first ),
            received( reply_to( first ) ) } );

        const auto records = rig.device_records();

        check_callbacks( records, { adv_timeout, connection_end_event{} } );

        const auto advertising_end = the_only( callbacks_of( records, adv_timeout ) );
        const auto end             = the_only( callbacks_of( records, callback_kind::connection_end_event ) );

        BOOST_CHECK_LE( std::chrono::abs( time_between( advertising_end, end ) - at ), tolerance );
    }

    /*
     * The same PDU where the device must not receive it, outside its receive window or on an
     * access address other than the connection's: the device does not answer, and the event
     * ends with connection_timeout() carrying `end`.
     */
    void a_pdu_is_not_received( connection_fixture& rig, std::chrono::microseconds at, std::uint32_t access_address )
    {
        const auto advertisement = advertising( 6, 0x01 );

        central tester_side;
        const auto first = tester_side.send();

        rig.program_device( {
            on_start(
                start_advertising( 37, advertisement ) ),
            on_adv_timeout(
                set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
                schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
            on_connection_timeout() } );

        rig.program_tester( {
            receive( 37, 1, operation_timeout ),
            use_access_address( access_address, connection_crc_init ),
            connection_event( data_channel, at, { first } ) } );

        rig.run();

        rig.check_captured( {
            received( advertisement ),
            sent( first ) } );

        const auto records = rig.device_records();

        check_callbacks( records, { adv_timeout, connection_timeout } );

        const auto advertising_end = the_only( callbacks_of( records, adv_timeout ) );
        const auto timeout         = the_only( callbacks_of( records, connection_timeout ) );

        BOOST_CHECK_EQUAL( time_between( advertising_end, timeout ), std::chrono::microseconds( event_start + receive_window ) );
    }

    // `size` bytes, each one different, so that a byte out of place shows
    std::vector< std::uint8_t > payload_of( std::size_t size )
    {
        std::vector< std::uint8_t > result( size );
        std::iota( result.begin(), result.end(), std::uint8_t( 1 ) );

        return result;
    }

    /*
     * The central sends a data PDU with `payload_size` bytes of payload. The device acknowledges
     * it one inter frame space after it ended and hands it to its link layer as it was received.
     */
    void a_data_pdu_of_the_central_is_received( connection_fixture& rig, std::size_t payload_size )
    {
        const auto advertisement = advertising( 6, 0x01 );
        const auto payload       = payload_of( payload_size );

        central tester_side;
        const auto data = tester_side.send( payload, no_more_data, llid::start );

        rig.program_device( {
            on_start(
                start_advertising( 37, advertisement ) ),
            on_adv_timeout(
                set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
                schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
            on_connection_end_event() } );

        rig.program_tester( {
            receive( 37, 1, operation_timeout ),
            use_access_address( connection_access_address, connection_crc_init ),
            connection_event( data_channel, first_pdu_after_advertising, { data } ) } );

        rig.run();

        const auto captured = rig.check_captured( {
            received( advertisement ),
            sent( data ),
            received( reply_to( data ) ) } );

        check_callbacks( rig.device_records(), { adv_timeout, connection_end_event{ .last_received_not_empty = true } } );

        BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 1 ], captured[ 2 ] ) - 150us ), 2us );

        const auto stored = rig.device_received();

        BOOST_REQUIRE_EQUAL( stored.size(), 1u );
        BOOST_CHECK( std::vector< std::uint8_t >( stored[ 0 ].data.begin(), stored[ 0 ].data.begin() + stored[ 0 ].size ) == data );
    }

    /*
     * The device has a data PDU with `payload_size` bytes of payload queued, and sends it as its
     * reply one inter frame space after the central's PDU ended.
     */
    void a_data_pdu_of_the_device_is_sent( connection_fixture& rig, std::size_t payload_size )
    {
        const auto advertisement = advertising( 6, 0x01 );
        const auto payload       = payload_of( payload_size );

        central tester_side;
        const auto first = tester_side.send();

        rig.queue_device_pdus( { data_pdu( llid::start, payload ) } );

        rig.program_device( {
            on_start(
                start_advertising( 37, advertisement ) ),
            on_adv_timeout(
                set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
                schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
            on_connection_end_event() } );

        rig.program_tester( {
            receive( 37, 1, operation_timeout ),
            use_access_address( connection_access_address, connection_crc_init ),
            connection_event( data_channel, first_pdu_after_advertising, { first } ) } );

        rig.run();

        const auto captured = rig.check_captured( {
            received( advertisement ),
            sent( first ),
            received( reply_to( first, payload, no_more_data, llid::start ) ) } );

        check_callbacks( rig.device_records(), {
            adv_timeout, connection_end_event{ .unacknowledged_data = true, .last_transmitted_not_empty = true } } );

        BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 1 ], captured[ 2 ] ) - 150us ), 2us );
    }

    /*
     * One event in which the central sends three PDUs, the first two with MD set, each after
     * the device's reply to the one before with `t_ifs` in between. The device has to follow at
     * the edges of the inter frame space the Core Specification allows, 150 µs ± 2 µs.
     */
    void the_device_follows_a_central_at( connection_fixture& rig, std::chrono::microseconds t_ifs )
    {
        const auto advertisement = advertising( 6, 0x01 );

        central tester_side;
        const auto first  = tester_side.send( {}, more_data );
        const auto second = tester_side.send( {}, more_data );
        const auto third  = tester_side.send();

        rig.program_device( {
            on_start(
                start_advertising( 37, advertisement ) ),
            on_adv_timeout(
                set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
                schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
            on_connection_end_event() } );

        rig.program_tester( {
            receive( 37, 1, operation_timeout ),
            use_access_address( connection_access_address, connection_crc_init ),
            connection_event( data_channel, first_pdu_after_advertising, { first, second, third }, t_ifs ) } );

        rig.run();

        const auto captured = rig.check_captured( {
            received( advertisement ),
            sent( first ),  received( reply_to( first ) ),
            sent( second ), received( reply_to( second ) ),
            sent( third ),  received( reply_to( third ) ) } );

        check_callbacks( rig.device_records(), { adv_timeout, connection_end_event{} } );

        // the tester kept the space it was asked to, or the test proves nothing
        BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 2 ], captured[ 3 ] ) - t_ifs ), 500ns );
        BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 4 ], captured[ 5 ] ) - t_ifs ), 500ns );
    }
}

// the central's empty PDU is answered with an empty one, and the event closes after it
BOOST_FIXTURE_TEST_CASE( an_empty_pdu_is_answered_with_an_empty_pdu, rig_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto from_central = tester_side.send();

    program_device( {
        on_start(
            start_advertising( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { from_central } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( from_central ),
        received( reply_to( from_central ) ) } );

    const auto records = device_records();

    check_callbacks( records, { adv_timeout, connection_end_event{} } );

    // the anchor is the first bit of the tester's PDU, by the device's clock
    const auto advertising_end = the_only( callbacks_of( records, adv_timeout ) );
    const auto end             = the_only( callbacks_of( records, callback_kind::connection_end_event ) );
    const auto anchor          = time_between( advertising_end, end );

    BOOST_CHECK_LE( std::chrono::abs( anchor - first_pdu_after_advertising ), tolerance );
}

/*
 * The device follows the central from event to event and from channel to channel. The anchors
 * are an interval apart by both clocks: the tester's, which placed them, and the device's,
 * which reports them.
 */
BOOST_FIXTURE_TEST_CASE( connection_events_follow_each_other_at_the_interval, connection_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send();
    const auto third  = tester_side.send();

    program_device( {
        on_start(
            start_advertising( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( 5, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            next_event( 12 ) ),
        on_connection_end_event(
            next_event( 19 ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( 5,  first_pdu_after_advertising, { first } ),
        connection_event( 12, interval, { second } ),
        connection_event( 19, interval, { third } ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ),
        sent( third ),  received( reply_to( third ) ) } );

    const auto records = device_records();

    check_callbacks( records, { adv_timeout, connection_end_event{}, connection_end_event{}, connection_end_event{} } );

    const auto ends = callbacks_of( records, callback_kind::connection_end_event );

    BOOST_REQUIRE_EQUAL( ends.size(), 3u );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 1 ], captured[ 3 ] ) - interval ), tolerance );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 3 ], captured[ 5 ] ) - interval ), tolerance );
    BOOST_CHECK_LE( std::chrono::abs( time_between( ends[ 0 ], ends[ 1 ] ) - interval ), tolerance );
    BOOST_CHECK_LE( std::chrono::abs( time_between( ends[ 1 ], ends[ 2 ] ) - interval ), tolerance );
}

/*
 * A PDU in the buffer before the connection goes out with the first reply. Its acknowledgement
 * can only come with the central's first PDU of the next event, so the first event ends with
 * the data unacknowledged, and the second with nothing left.
 */
BOOST_FIXTURE_TEST_CASE( data_queued_before_the_start_is_sent_in_the_first_event, connection_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send();

    queue_device_pdus( { data_pdu( llid::start, some_data ) } );

    program_device( {
        on_start(
            start_advertising( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first, some_data, no_more_data, llid::start ) ),
        sent( second ), received( reply_to( second ) ) } );

    check_callbacks( device_records(), {
        adv_timeout,
        connection_end_event{ .unacknowledged_data = true, .last_transmitted_not_empty = true },
        connection_end_event{} } );
}

// what a link layer does from its callback: fill the buffer for the next event
BOOST_FIXTURE_TEST_CASE( data_queued_after_an_event_is_sent_in_the_next, connection_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send();

    program_device( {
        on_start(
            start_advertising( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            queue_pdu( data_pdu( llid::start, some_data ) ),
            next_event( data_channel ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second, some_data, no_more_data, llid::start ) ) } );

    check_callbacks( device_records(), {
        adv_timeout,
        connection_end_event{},
        connection_end_event{ .unacknowledged_data = true, .last_transmitted_not_empty = true } } );
}

/*
 * The central's MD keeps the device listening after its reply. The last PDU of the event has
 * MD clear, so last_received_had_more_data, which is about the last PDU received, is clear.
 */
BOOST_FIXTURE_TEST_CASE( more_data_of_the_central_keeps_the_event_open, connection_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send( {}, more_data );
    const auto second = tester_side.send();

    program_device( {
        on_start(
            start_advertising( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first, second } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ) } );

    check_callbacks( device_records(), { adv_timeout, connection_end_event{} } );
}

/*
 * The tester places a PDU to one tick of its clock, but finds the end of the device's reply
 * from a receive offset calibrated against this device's own T_IFS; until the tester is
 * validated against an independent reference, this shows that the device follows at 148 µs
 * and 152 µs as the tester measures them (candidate_tests.md).
 */
BOOST_FIXTURE_TEST_CASE( the_device_follows_a_central_at_the_shortest_inter_frame_space, connection_fixture, *if_tester )
{
    the_device_follows_a_central_at( *this, 148us );
}

BOOST_FIXTURE_TEST_CASE( the_device_follows_a_central_at_the_longest_inter_frame_space, connection_fixture, *if_tester )
{
    the_device_follows_a_central_at( *this, 152us );
}

/*
 * Payloads of one byte, a middle size and 27 bytes, the largest without the data length
 * extension, in both directions (candidate_tests.md).
 */
BOOST_FIXTURE_TEST_CASE( the_smallest_data_pdu_of_the_central_is_received, connection_fixture, *if_tester )
{
    a_data_pdu_of_the_central_is_received( *this, 1 );
}

BOOST_FIXTURE_TEST_CASE( a_middle_sized_data_pdu_of_the_central_is_received, connection_fixture, *if_tester )
{
    a_data_pdu_of_the_central_is_received( *this, 13 );
}

BOOST_FIXTURE_TEST_CASE( the_largest_data_pdu_of_the_central_is_received, connection_fixture, *if_tester )
{
    a_data_pdu_of_the_central_is_received( *this, 27 );
}

BOOST_FIXTURE_TEST_CASE( the_smallest_data_pdu_of_the_device_is_sent, connection_fixture, *if_tester )
{
    a_data_pdu_of_the_device_is_sent( *this, 1 );
}

BOOST_FIXTURE_TEST_CASE( a_middle_sized_data_pdu_of_the_device_is_sent, connection_fixture, *if_tester )
{
    a_data_pdu_of_the_device_is_sent( *this, 13 );
}

BOOST_FIXTURE_TEST_CASE( the_largest_data_pdu_of_the_device_is_sent, connection_fixture, *if_tester )
{
    a_data_pdu_of_the_device_is_sent( *this, 27 );
}

/*
 * The edges of the device's receive window, from `start` to `end` of the event: a PDU is received
 * if its first bit is on air between them. The margin of 20 µs covers the drift of two stock
 * crystals over the 50 ms both sides place the event from the advertising.
 */
BOOST_FIXTURE_TEST_CASE( a_pdu_before_the_receive_window_is_not_received, connection_fixture, *if_tester )
{
    a_pdu_is_not_received( *this, event_start - 200us, connection_access_address );
}

BOOST_FIXTURE_TEST_CASE( a_pdu_just_after_the_receive_window_opened_is_received, connection_fixture, *if_tester )
{
    a_pdu_is_received( *this, event_start + 20us );
}

BOOST_FIXTURE_TEST_CASE( a_pdu_just_before_the_receive_window_closes_is_received, connection_fixture, *if_tester )
{
    a_pdu_is_received( *this, event_start + receive_window - 20us );
}

BOOST_FIXTURE_TEST_CASE( a_pdu_after_the_receive_window_closed_is_not_received, connection_fixture, *if_tester )
{
    a_pdu_is_not_received( *this, event_start + receive_window + 100us, connection_access_address );
}

// the window's end must not make a reception of what it does not match
BOOST_FIXTURE_TEST_CASE( a_pdu_on_another_access_address_is_not_received, connection_fixture, *if_tester )
{
    a_pdu_is_not_received( *this, first_pdu_after_advertising, other_access_address );
}
