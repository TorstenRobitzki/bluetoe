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

#include "test_tools/dut.hpp"
#include "test_tools/observations.hpp"
#include "test_tools/records.hpp"
#include "test_tools/rig_fixture.hpp"
#include "test_tools/tester.hpp"

#include <chrono>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    // how long a tester operation may wait for what it waits for
    const time_out operation_timeout{ 300ms };

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
            receive( 37, 1, operation_timeout ),
            listen( 37, 300ms ) } );

        rig.run();

        rig.check_captured( { received( first ) } );

        const auto records = rig.device_records();

        BOOST_CHECK( the_only( calls_of( records, call_kind::cancel_radio_event ) ).result );
        check_callbacks( records, { adv_timeout } );
    }
}

BOOST_FIXTURE_TEST_CASE( a_scheduled_advertising_event_cancelled_in_time_is_not_sent, rig_fixture, *if_tester )
{
    const auto first     = advertising( 7, 0x01 );
    const auto cancelled = advertising( 7, 0x02 );

    cancelled_in_time( *this, schedule_advertising_event( 37, 100ms, cancelled ), first );
}

// the cancel follows the start by microseconds, before the transmitter was started
BOOST_FIXTURE_TEST_CASE( a_started_advertising_cancelled_in_time_is_not_sent, rig_fixture, *if_tester )
{
    const auto first     = advertising( 7, 0x01 );
    const auto cancelled = advertising( 7, 0x02 );

    cancelled_in_time( *this, start_advertising( 37, cancelled ), first );
}

// once the callback was delivered, nothing is pending
BOOST_FIXTURE_TEST_CASE( cancelling_with_nothing_pending_is_refused, rig_fixture, *if_tester )
{
    program_device( {
        on_start(       start_advertising( 37, advertising( 7, 0x01 ) ) ),
        on_adv_timeout( cancel_radio_event() ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ) } );

    run();

    BOOST_CHECK( !the_only( calls_of( device_records(), call_kind::cancel_radio_event ) ).result );
}

/*
 * The timer expires at the time the event's first bit is on air, so its transmitter is ramping
 * up or transmitting already: the cancel records false, and the event proceeds with its callback.
 */
BOOST_FIXTURE_TEST_CASE( a_cancel_too_late_lets_the_event_proceed, rig_fixture, *if_tester )
{
    const auto first = advertising( 7, 0x01 );
    const auto late  = advertising( 7, 0x02 );

    program_device( {
        on_start(       start_advertising( 37, first ) ),
        on_adv_timeout( schedule_advertising_event( 37, 100ms, late ),
                        schedule_timer( 100ms ) ),
        on_user_timer(  cancel_radio_event() ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        receive( 37, 1, operation_timeout ) } );

    run();

    const auto captured = check_captured( {
        received( first ),
        received( late ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 0 ], captured[ 1 ] ) - 100ms ), tolerance );

    const auto records = device_records();

    BOOST_CHECK( !the_only( calls_of( records, call_kind::cancel_radio_event ) ).result );
    check_callbacks( records, { adv_timeout, user_timer, adv_timeout } );
}

/*
 * A cancel in time from a timer leaves nothing pending, so the step can start advertising
 * again at once: the tester hears the restart about 50 ms after `first`, and the cancelled
 * event not at 100 ms.
 */
BOOST_FIXTURE_TEST_CASE( a_cancel_leaves_the_radio_free_for_the_next_action, rig_fixture, *if_tester )
{
    const auto first     = advertising( 7, 0x01 );
    const auto cancelled = advertising( 7, 0x02 );
    const auto restarted = advertising( 7, 0x03 );

    program_device( {
        on_start(       start_advertising( 37, first ) ),
        on_adv_timeout( schedule_advertising_event( 37, 100ms, cancelled ),
                        schedule_timer( 50ms ) ),
        on_user_timer(  cancel_radio_event(),
                        start_advertising( 37, restarted ) ) } );

    // the listen covers the time the cancelled event was scheduled for
    program_tester( {
        receive( 37, 1, operation_timeout ),
        receive( 37, 1, operation_timeout ),
        listen( 37, 100ms ) } );

    run();

    const auto captured = check_captured( {
        received( first ),
        received( restarted ) } );

    const auto between = time_between( captured[ 0 ], captured[ 1 ] );

    BOOST_CHECK_GE( between, 50ms );
    BOOST_CHECK_LT( between, 100ms );

    const auto records = device_records();

    BOOST_CHECK( the_only( calls_of( records, call_kind::cancel_radio_event ) ).result );
    check_callbacks( records, { adv_timeout, user_timer, adv_timeout } );
}
