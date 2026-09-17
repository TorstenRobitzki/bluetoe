/**
 * @file cancel_tests.cpp
 *
 * cancel_radio_event() on the advertising events of start_advertising() and
 * schedule_advertising_event(). A step runs on a callback only, so a cancel in time is made
 * in the step that scheduled the event, or from a timer before it; a cancel too late from a
 * timer when the event goes on air. Needs the tester; skipped without one.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "radio_tests/dut.hpp"
#include "radio_tests/observations.hpp"
#include "radio_tests/rig_fixture.hpp"
#include "radio_tests/tester.hpp"

#include <bluetoe/delta_time.hpp>

#include <cstdlib>
#include <vector>

using namespace bluetoe::test_rig;
using bluetoe::link_layer::delta_time;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    /*
     * The tester hears `first` and then, for a window past the cancelled event's time,
     * nothing; the cancel records true, and no callback follows `first`'s adv_timeout.
     */
    void cancelled_in_time( rig_fixture& rig, const call& schedule, const std::vector< std::uint8_t >& first )
    {
        rig.program_device( {
            on_start(       start_advertising( 37, first ) ),
            on_adv_timeout( schedule, cancel_radio_event() ) } );

        rig.program_tester( {
            receive( 37, delta_time::msec( 300 ), 1 ),
            receive( 37, delta_time::msec( 300 ) ) } );

        rig.run();

        const auto captured = rig.tester_captured();

        BOOST_REQUIRE_EQUAL( captured.size(), 1u );
        BOOST_CHECK( carries( captured[ 0 ], first ) );

        const auto records   = rig.device_records();
        const auto cancelled = calls_of( records, call_kind::cancel_radio_event );

        BOOST_REQUIRE_EQUAL( cancelled.size(), 1u );
        BOOST_CHECK( cancelled[ 0 ].result );
        BOOST_CHECK_EQUAL( callbacks_of( records, callback_kind::adv_timeout ).size(), 1u );
        BOOST_CHECK( callbacks_of( records, callback_kind::adv_received ).empty() );
    }
}

BOOST_FIXTURE_TEST_CASE( a_scheduled_advertising_event_cancelled_in_time_is_not_sent, rig_fixture, *if_tester )
{
    const auto first     = advertising( 6, 0x01 );
    const auto cancelled = advertising( 6, 0x02 );

    cancelled_in_time( *this, schedule_advertising_event( 37, delta_time::msec( 100 ), cancelled ), first );
}

// the cancel follows the start by microseconds, before the transmitter was started
BOOST_FIXTURE_TEST_CASE( a_started_advertising_cancelled_in_time_is_not_sent, rig_fixture, *if_tester )
{
    const auto first     = advertising( 6, 0x01 );
    const auto cancelled = advertising( 6, 0x02 );

    cancelled_in_time( *this, start_advertising( 37, cancelled ), first );
}

// once the callback was delivered, nothing is pending
BOOST_FIXTURE_TEST_CASE( cancelling_with_nothing_pending_is_refused, rig_fixture, *if_tester )
{
    program_device( {
        on_start(       start_advertising( 37, advertising( 6, 0x01 ) ) ),
        on_adv_timeout( cancel_radio_event() ) } );

    program_tester( {
        receive( 37, delta_time::msec( 300 ), 1 ) } );

    run();

    const auto cancelled = calls_of( device_records(), call_kind::cancel_radio_event );

    BOOST_REQUIRE_EQUAL( cancelled.size(), 1u );
    BOOST_CHECK( !cancelled[ 0 ].result );
}

/*
 * The timer expires at the time the event's first bit is on air, so its transmitter is ramping
 * up or transmitting already: the cancel records false, and the event proceeds with its callback.
 */
BOOST_FIXTURE_TEST_CASE( a_cancel_too_late_lets_the_event_proceed, rig_fixture, *if_tester )
{
    const auto first = advertising( 6, 0x01 );
    const auto late  = advertising( 6, 0x02 );

    program_device( {
        on_start(       start_advertising( 37, first ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), late ),
                        schedule_timer( delta_time::msec( 100 ) ) ),
        on_user_timer(  cancel_radio_event() ) } );

    program_tester( {
        receive( 37, delta_time::msec( 300 ), 1 ),
        receive( 37, delta_time::msec( 300 ), 1 ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 2u );
    BOOST_CHECK( carries( captured[ 1 ], late ) );
    BOOST_CHECK_LE( std::abs( microseconds_between( captured[ 0 ], captured[ 1 ] ) - 100'000 ), tolerance_us );

    const auto records   = device_records();
    const auto cancelled = calls_of( records, call_kind::cancel_radio_event );

    BOOST_REQUIRE_EQUAL( cancelled.size(), 1u );
    BOOST_CHECK( !cancelled[ 0 ].result );
    BOOST_CHECK_EQUAL( callbacks_of( records, callback_kind::adv_timeout ).size(), 2u );
}

/*
 * A cancel in time from a timer leaves nothing pending, so the step can start advertising
 * again at once: the tester hears the restart about 50 ms after `first`, and the cancelled
 * event not at 100 ms.
 */
BOOST_FIXTURE_TEST_CASE( a_cancel_leaves_the_radio_free_for_the_next_action, rig_fixture, *if_tester )
{
    const auto first     = advertising( 6, 0x01 );
    const auto cancelled = advertising( 6, 0x02 );
    const auto restarted = advertising( 6, 0x03 );

    program_device( {
        on_start(       start_advertising( 37, first ) ),
        on_adv_timeout( schedule_advertising_event( 37, delta_time::msec( 100 ), cancelled ),
                        schedule_timer( delta_time::msec( 50 ) ) ),
        on_user_timer(  cancel_radio_event(),
                        start_advertising( 37, restarted ) ) } );

    // the last receive covers the time the cancelled event was scheduled for
    program_tester( {
        receive( 37, delta_time::msec( 300 ), 1 ),
        receive( 37, delta_time::msec( 300 ), 1 ),
        receive( 37, delta_time::msec( 100 ) ) } );

    run();

    const auto captured = tester_captured();

    BOOST_REQUIRE_EQUAL( captured.size(), 2u );
    BOOST_CHECK( carries( captured[ 1 ], restarted ) );

    const long between_us = microseconds_between( captured[ 0 ], captured[ 1 ] );

    BOOST_CHECK_GE( between_us, 50'000 );
    BOOST_CHECK_LT( between_us, 100'000 );

    const auto records         = device_records();
    const auto cancelled_calls = calls_of( records, call_kind::cancel_radio_event );

    BOOST_REQUIRE_EQUAL( cancelled_calls.size(), 1u );
    BOOST_CHECK( cancelled_calls[ 0 ].result );
    BOOST_CHECK_EQUAL( callbacks_of( records, callback_kind::adv_timeout ).size(), 2u );
}
