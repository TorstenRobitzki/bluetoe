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

#include "radio_tests/dut.hpp"
#include "radio_tests/observations.hpp"
#include "radio_tests/rig_fixture.hpp"
#include "radio_tests/tester.hpp"

#include "host/central.hpp"

#include <chrono>
#include <cstdint>
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

    // the connection event starts this long after the advertising, and receives until
    // `receive_window` later without a reception
    constexpr auto event_start    = 50ms;
    constexpr auto receive_window = 2ms;

    // the tester's first PDU of the event begins this long after the advertising, 500 µs into
    // the receive window
    constexpr auto first_pdu_after_advertising = event_start + 500us;
}

// the central's empty PDU is answered with an empty one, and the event closes after it
BOOST_FIXTURE_TEST_CASE( an_empty_pdu_is_answered_with_an_empty_pdu, rig_fixture, *if_tester )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto sent = tester_side.send();

    program_device( {
        on_start(
            start_advertising( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        transmit( data_channel, first_pdu_after_advertising, sent, operation_timeout ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 3u );
    BOOST_CHECK( carries( captured[ 1 ], sent ) );

    const captured_pdu& reply = captured[ 2 ];

    BOOST_CHECK( reply.crc_ok );
    BOOST_CHECK_EQUAL( reply.data.size, 2u );
    BOOST_CHECK_EQUAL( reply.data.data[ 0 ] & 0x03, 0x01 );
    BOOST_CHECK( acknowledges( reply, sent ) );
    BOOST_CHECK( is_new( reply, sent ) );
    BOOST_CHECK( !more_data( std::span< const std::uint8_t >( reply.data.data.data(), reply.data.size ) ) );

    const auto records = device_records();
    const auto ends    = callbacks_of( records, callback_kind::connection_end_event );

    BOOST_REQUIRE_EQUAL( ends.size(), 1u );
    BOOST_CHECK( callbacks_of( records, callback_kind::connection_timeout ).empty() );
    BOOST_CHECK( !ends[ 0 ].events.last_received_not_empty );
    BOOST_CHECK( !ends[ 0 ].events.last_transmitted_not_empty );
    BOOST_CHECK( !ends[ 0 ].events.error_occured );

    // the anchor is the first bit of the tester's PDU, by the device's clock
    const auto advertising_end = callbacks_of( records, callback_kind::adv_timeout );

    BOOST_REQUIRE_EQUAL( advertising_end.size(), 1u );

    const auto anchor = time_between( advertising_end[ 0 ], ends[ 0 ] );

    BOOST_CHECK_LE( std::chrono::abs( anchor - first_pdu_after_advertising ), tolerance );
}
