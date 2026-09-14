/**
 * @file first_tests.cpp
 *
 * The first two timing tests of a scheduled radio: that an advertising event is transmitted
 * at the requested time, and on the requested channel. Both need the tester; they are
 * skipped without one. See documentation/scheduled_radio_test_rig.md.
 *
 * These run over the air, so the tester hears every advertiser on the channel and the
 * device's own advertising window occasionally catches one, which ends the program a run
 * short. The tests tell the device's PDUs from the air's by their content and tolerate a
 * missed one; a program the stray reception stalls fails that run, and the run is repeated.
 * Ruling that out for good needs the boards coupled by cable with their antennas switched
 * out, which is a bench setup, not a code change.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "radio_tests/dut.hpp"
#include "radio_tests/rig_fixture.hpp"
#include "radio_tests/tester.hpp"

#include <bluetoe/delta_time.hpp>

#include <chrono>
#include <cstdint>
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

    // a distinctive, non-empty payload, so the tester tells the device's PDUs from the air's
    const std::uint8_t marker[]       = { 0x02, 0x06, 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0 };
    const std::uint8_t other_marker[] = { 0x02, 0x06, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb };
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

    const auto seen = received_matching( marker );

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

    const auto on_38 = received_matching( marker );
    const auto stray = received_matching( other_marker );

    BOOST_CHECK_GE( on_38.size(), 1u );
    // the channel-37 PDU must not appear on channel 38
    BOOST_CHECK_EQUAL( stray.size(), 0u );
}
