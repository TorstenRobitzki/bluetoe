/**
 * @file timer_tests.cpp
 *
 * schedule_timer() and cancel_timer(). The timer does not transmit, so a step on user_timer
 * schedules an advertising that the tester observes, and the device's records show the times
 * the calls resolved to and the callbacks carried. Needs the tester; skipped without one.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "radio_tests/dut.hpp"
#include "radio_tests/observations.hpp"
#include "radio_tests/records.hpp"
#include "radio_tests/rig_fixture.hpp"
#include "radio_tests/tester.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    // how long a tester operation may wait for what it waits for
    const time_out operation_timeout{ 300ms };

    // the index of the first record that is the callback `kind`
    std::size_t index_of( const std::vector< record >& records, callback_kind kind )
    {
        const auto found = std::find_if( records.begin(), records.end(),
            [ kind ]( const record& r ){ return r.kind == record_kind::callback && r.callback == kind; } );

        return static_cast< std::size_t >( found - records.begin() );
    }
}

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

    const auto records   = device_records();
    const auto timeouts  = callbacks_of( records, callback_kind::adv_timeout );
    const auto scheduled = calls_of( records, call_kind::schedule_timer );
    const auto expired   = callbacks_of( records, callback_kind::user_timer );

    BOOST_REQUIRE( !timeouts.empty() );
    BOOST_REQUIRE_EQUAL( scheduled.size(), 1u );
    BOOST_REQUIRE_EQUAL( expired.size(), 1u );
    BOOST_CHECK( scheduled[ 0 ].result );
    BOOST_CHECK_EQUAL( time_between( timeouts[ 0 ], scheduled[ 0 ] ), timer_delay );
    // abs_time compares by proximity; these are the same instant
    BOOST_CHECK_EQUAL( expired[ 0 ].when.data(), scheduled[ 0 ].when.data() );
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

    // the first adv_timeout, then the timer, then the second event's adv_timeout
    auto records = device_records();

    const std::size_t first_timeout = index_of( records, callback_kind::adv_timeout );
    const std::size_t timer         = index_of( records, callback_kind::user_timer );

    BOOST_REQUIRE_LT( timer, records.size() );
    BOOST_CHECK_LT( first_timeout, timer );

    records.erase( records.begin(), records.begin() + timer );
    BOOST_CHECK_LT( index_of( records, callback_kind::adv_timeout ), records.size() );
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

    const auto records   = device_records();
    const auto scheduled = calls_of( records, call_kind::schedule_timer );

    BOOST_REQUIRE_EQUAL( scheduled.size(), 1u );
    BOOST_CHECK( !scheduled[ 0 ].result );
    BOOST_CHECK( callbacks_of( records, callback_kind::user_timer ).empty() );
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

    const auto records   = device_records();
    const auto scheduled = calls_of( records, call_kind::schedule_timer );
    const auto cancelled = calls_of( records, call_kind::cancel_timer );

    BOOST_REQUIRE_EQUAL( scheduled.size(), 1u );
    BOOST_REQUIRE_EQUAL( cancelled.size(), 1u );
    BOOST_CHECK( scheduled[ 0 ].result );
    BOOST_CHECK( cancelled[ 0 ].result );
    BOOST_CHECK( callbacks_of( records, callback_kind::user_timer ).empty() );
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
    BOOST_CHECK_EQUAL( callbacks_of( records, callback_kind::user_timer ).size(), 1u );
}
