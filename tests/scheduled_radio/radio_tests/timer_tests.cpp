/**
 * @file timer_tests.cpp
 *
 * schedule_timer() and cancel_timer(). The timer does not transmit, so a step on user_timer
 * schedules an advertising that the tester observes, and the device's records show the times
 * the calls resolved to and the callbacks carried. Needs the tester; skipped without one.
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

/*
 * user_timer() carries the time the timer was scheduled for, and a step on it places an
 * advertising from that time.
 */
BOOST_FIXTURE_TEST_CASE( a_timer_expires_with_the_time_it_was_scheduled_for, rig_fixture, *if_tester )
{
    constexpr auto timer_delay = 100ms;
    constexpr auto event_delay = 10ms;

    const auto first  = advertising( 7, 0x01 );
    const auto second = advertising( 7, 0x02 );

    program_device( {
        on_start(       start_advertising( 37, first ) ),
        on_adv_timeout( schedule_timer( timer_delay ) ),
        on_user_timer(  schedule_advertising_event( 37, event_delay, second ) ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        receive( 37, 1, operation_timeout ) } );

    run();

    const auto captured = check_captured( {
        received( first ),
        received( second ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 0 ], captured[ 1 ] ) - ( timer_delay + event_delay ) ), tolerance );

    const auto records = device_records();

    check_callbacks( records, { adv_timeout, user_timer, adv_timeout } );

    const auto timeouts  = callbacks_of( records, adv_timeout );
    const auto scheduled = the_only( calls_of( records, call_kind::schedule_timer ) );
    const auto expired   = the_only( callbacks_of( records, user_timer ) );

    BOOST_REQUIRE( !timeouts.empty() );
    BOOST_CHECK( scheduled.result );
    BOOST_CHECK_EQUAL( time_between( timeouts[ 0 ], scheduled ), timer_delay );
    // abs_time compares by proximity; these are the same instant
    BOOST_CHECK_EQUAL( expired.when.data(), scheduled.when.data() );
}

// a timer expires between the events, and the advertising event keeps its time
BOOST_FIXTURE_TEST_CASE( a_timer_and_a_pending_advertising_event_do_not_disturb_each_other, rig_fixture, *if_tester )
{
    const auto first  = advertising( 7, 0x01 );
    const auto second = advertising( 7, 0x02 );

    program_device( {
        on_start(       start_advertising( 37, first ) ),
        on_adv_timeout( schedule_advertising_event( 37, 100ms, second ),
                        schedule_timer( 50ms ) ),
        on_user_timer() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        receive( 37, 1, operation_timeout ) } );

    run();

    const auto captured = check_captured( {
        received( first ),
        received( second ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 0 ], captured[ 1 ] ) - 100ms ), tolerance );

    // the timer expires between the two events
    check_callbacks( device_records(), { adv_timeout, user_timer, adv_timeout } );
}

// the time the callback carried is gone by when its step runs
BOOST_FIXTURE_TEST_CASE( a_timer_for_a_time_gone_by_is_refused, rig_fixture, *if_tester )
{
    program_device( {
        on_start(       start_advertising( 37, advertising( 7, 0x01 ) ) ),
        on_adv_timeout( schedule_timer( 0ms ) ) } );

    // the listen only keeps the run going while a wrongly scheduled timer would expire
    program_tester( {
        receive( 37, 1, operation_timeout ),
        listen( 37, 100ms ) } );

    run();

    const auto records = device_records();

    BOOST_CHECK( !the_only( calls_of( records, call_kind::schedule_timer ) ).result );
    check_callbacks( records, { adv_timeout } );
}

BOOST_FIXTURE_TEST_CASE( a_timer_cancelled_in_time_does_not_expire, rig_fixture, *if_tester )
{
    program_device( {
        on_start(       start_advertising( 37, advertising( 7, 0x01 ) ) ),
        on_adv_timeout( schedule_timer( 100ms ),
                        cancel_timer() ) } );

    // the listen keeps the run going past the time the timer was scheduled for
    program_tester( {
        receive( 37, 1, operation_timeout ),
        listen( 37, 300ms ) } );

    run();

    const auto records = device_records();

    BOOST_CHECK( the_only( calls_of( records, call_kind::schedule_timer ) ).result );
    BOOST_CHECK( the_only( calls_of( records, call_kind::cancel_timer ) ).result );
    check_callbacks( records, { adv_timeout } );
}

// before any timer was scheduled, and after the one scheduled was delivered
BOOST_FIXTURE_TEST_CASE( cancelling_without_a_scheduled_timer_is_refused, rig_fixture, *if_tester )
{
    program_device( {
        on_start(       start_advertising( 37, advertising( 7, 0x01 ) ) ),
        on_adv_timeout( cancel_timer(),
                        schedule_timer( 50ms ) ),
        on_user_timer(  cancel_timer() ) } );

    program_tester( {
        receive( 37, 1, operation_timeout ) } );

    run();

    const auto records   = device_records();
    const auto cancelled = calls_of( records, call_kind::cancel_timer );

    BOOST_REQUIRE_EQUAL( cancelled.size(), 2u );
    BOOST_CHECK( !cancelled[ 0 ].result );
    BOOST_CHECK( !cancelled[ 1 ].result );
    check_callbacks( records, { adv_timeout, user_timer } );
}
