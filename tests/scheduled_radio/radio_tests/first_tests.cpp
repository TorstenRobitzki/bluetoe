/**
 * @file first_tests.cpp
 *
 * The first two timing tests of a scheduled radio: that an advertising event is transmitted
 * at the requested time, and on the requested channel. Both need the tester; they are
 * skipped without one. See documentation/scheduled_radio_test_rig.md.
 *
 * These run over the air, but the acceptance filter on each side keeps the air off the
 * record: the device answers only the tester, so a stray advertising in its window no longer
 * stalls the program, and the tester reports only the device, so what it hears is the
 * device's (scheduled_radio2.hpp). The two markers share the device's address and differ in
 * one byte, so both pass the tester's filter and the channel test still tells them apart.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "radio_tests/dut.hpp"
#include "radio_tests/rig_fixture.hpp"
#include "radio_tests/tester.hpp"

#include <bluetoe/delta_time.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;
using bluetoe::link_layer::delta_time;
using namespace std::chrono_literals;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    const delta_time event_interval = delta_time::msec( 100 );
    const delta_time listen_window  = delta_time::msec( 500 );

    // how far an observed interval may sit off the requested grid: the device's placement,
    // and the drift of the two crystals over an interval or two. Generous until the
    // reworked oscillator lets it be tightened.
    constexpr long   tolerance_us = 50;

    // an advertising PDU from the device: ADV_NONCONN_IND, the device's address (dut_address
    // of rig_fixture.hpp, what the tester's acceptance filter keeps), then one byte of data.
    // Both markers share the address, so both pass the filter; the last byte tells the two
    // of the device's PDUs apart, which is what the channel test needs.
    const std::uint8_t marker[]       = { 0x02, 0x07, 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0, 0x01 };
    const std::uint8_t other_marker[] = { 0x02, 0x07, 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0, 0x02 };

    // whether a received PDU carries exactly these bytes; the channel test tells the two
    // markers apart by it, since both share the address the tester filters on
    bool carries( const captured_pdu& p, std::span< const std::uint8_t > bytes )
    {
        return p.data.size == bytes.size()
            && std::equal( bytes.begin(), bytes.end(), p.data.data.begin() );
    }
}

/*
 * The device transmits on a fixed interval; the tester observes the transmissions and their
 * spacing must be that interval. A PDU the tester missed, because a background advertiser
 * collided with it, shows as a doubled gap rather than a failure, so the test asks only that
 * every observed gap sits on the requested grid: a whole number of intervals, within
 * tolerance (the find-in-range of the setup, since absence cannot be proven on the open air).
 */
BOOST_FIXTURE_TEST_CASE( advertising_is_transmitted_at_the_requested_time, rig_fixture, *if_tester )
{
    program_device( {
        on_start(       start_advertising(          37,                 marker ) ),
        on_adv_timeout( schedule_advertising_event( 37, event_interval, marker ) ),
        on_adv_timeout( schedule_advertising_event( 37, event_interval, marker ) ),
        on_adv_timeout( schedule_advertising_event( 37, event_interval, marker ) ) } );
    program_tester( { receive( 37, listen_window ) } );

    run();

    // the tester's acceptance filter keeps only the device, so all it reports is the device's
    const auto seen = tester_captured();

    BOOST_TEST_MESSAGE( "tester received " << seen.size() << " of the device's advertisings" );
    BOOST_REQUIRE_GE( seen.size(), 2u );

    const long requested_us = event_interval.usec();

    for ( std::size_t i = 1; i != seen.size(); ++i )
    {
        const long observed_us = std::chrono::duration_cast< std::chrono::microseconds >(
            time_of( seen[ i ].when ) - time_of( seen[ i - 1 ].when ) ).count();

        const long intervals = ( observed_us + requested_us / 2 ) / requested_us;
        const long residual  = observed_us - intervals * requested_us;

        BOOST_TEST_MESSAGE( "  gap " << observed_us << " us = " << intervals
            << " x interval, off by " << residual << " us" );

        BOOST_CHECK_GE( intervals, 1 );
        BOOST_CHECK_LE( residual < 0 ? -residual : residual, tolerance_us );
    }
}

BOOST_FIXTURE_TEST_CASE( advertising_is_transmitted_on_the_requested_channel, rig_fixture, *if_tester )
{
    // the device transmits its marker on channel 38; the tester listening there must hear it
    program_device( {
        on_start(       start_advertising(          37,                 other_marker ) ),
        on_adv_timeout( schedule_advertising_event( 38, event_interval, marker ) ) } );
    program_tester( { receive( 38, listen_window ) } );

    run();

    const auto received = tester_captured();

    const auto count_of = [ & ]( std::span< const std::uint8_t > bytes ) {
        return std::count_if( received.begin(), received.end(),
            [ & ]( const captured_pdu& p ){ return carries( p, bytes ); } );
    };

    BOOST_CHECK_GE( count_of( marker ), 1 );
    // the channel-37 PDU must not appear on channel 38
    BOOST_CHECK_EQUAL( count_of( other_marker ), 0 );
}
