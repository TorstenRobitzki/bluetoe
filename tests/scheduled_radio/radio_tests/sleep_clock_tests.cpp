/**
 * @file sleep_clock_tests.cpp
 *
 * Radio events and timers mixed, over idle stretches long enough for the radio to be down
 * to its sleep clock in between: a radio that switches its high frequency clock on for an
 * event and off after it has to place the next event, and the timers around it, from the
 * sleep clock alone, and a timer that expires while an event runs must not disturb it.
 * The tests hold on any sleep clock; on a radio that keeps the crystal on they check the
 * same placements without the switching. Needs the tester; skipped without one.
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
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

/*
 * Three advertising events, each placed from a timer that expired while the radio was idle
 * for a third of a second: the crystal is off during the wait and started for each event.
 */
BOOST_FIXTURE_TEST_CASE( advertising_events_placed_from_timers_over_long_idle_stretches, rig_fixture, *if_tester )
{
    constexpr auto idle        = 300ms;
    constexpr auto event_delay = 10ms;

    const auto advertisement = advertising( 7, 0x01 );
    const time_out longer_than_idle{ 500ms };

    program_device( {
        on_start(       start_advertising_event( 37, advertisement ) ),
        on_adv_timeout( schedule_timer( idle ) ),
        on_user_timer(  schedule_advertising_event( 37, event_delay, advertisement ) ),
        on_adv_timeout( schedule_timer( idle ) ),
        on_user_timer(  schedule_advertising_event( 37, event_delay, advertisement ) ),
        on_adv_timeout() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        receive( 37, 1, longer_than_idle ),
        receive( 37, 1, longer_than_idle ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        received( advertisement ),
        received( advertisement ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 0 ], captured[ 1 ] ) - ( idle + event_delay ) ), tolerance_for( idle + event_delay ) );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 1 ], captured[ 2 ] ) - ( idle + event_delay ) ), tolerance_for( idle + event_delay ) );

    check_callbacks( device_records(), { adv_timeout, user_timer, adv_timeout, user_timer, adv_timeout } );
}

/*
 * A timer expires in the middle of a connection event that runs for a few PDUs: the event
 * goes on undisturbed, every PDU is answered, and the timer is delivered with its time.
 */
BOOST_FIXTURE_TEST_CASE( a_timer_expiring_inside_a_connection_event_does_not_disturb_it, connection_fixture, *if_tester )
{
    constexpr auto into_the_event = 500us;

    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send( {}, more_data );
    const auto second = tester_side.send( {}, more_data );
    const auto third  = tester_side.send( {}, more_data );
    const auto fourth = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_timer( event_start + into_the_event ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_user_timer(),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first, second, third, fourth } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ),
        sent( third ),  received( reply_to( third ) ),
        sent( fourth ), received( reply_to( fourth ) ) } );

    const auto records = device_records();

    check_callbacks( records, { adv_timeout, user_timer, connection_end_event{} } );

    const auto advertising_end = the_only( callbacks_of( records, adv_timeout ) );
    const auto expired         = the_only( callbacks_of( records, user_timer ) );

    BOOST_CHECK_EQUAL( time_between( advertising_end, expired ), std::chrono::microseconds( event_start + into_the_event ) );
}

/*
 * Connection events at the interval with a timer between each pair, expiring while the
 * radio is idle: the events keep their interval, and each timer comes before the event
 * after it.
 */
BOOST_FIXTURE_TEST_CASE( connection_events_with_timers_between_them_keep_their_interval, connection_fixture, *if_tester )
{
    interval = 20ms;
    constexpr auto timer_delay = 5ms;

    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send();
    const auto third  = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            schedule_timer( timer_delay ),
            next_event( data_channel ) ),
        on_user_timer(),
        on_connection_end_event(
            schedule_timer( timer_delay ),
            next_event( data_channel ) ),
        on_user_timer(),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ),
        connection_event( data_channel, interval, { third } ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ),
        sent( third ),  received( reply_to( third ) ) } );

    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 1 ], captured[ 3 ] ) - interval ), tolerance_for( interval ) );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 3 ], captured[ 5 ] ) - interval ), tolerance_for( interval ) );

    check_callbacks( device_records(), {
        adv_timeout,
        connection_end_event{}, user_timer,
        connection_end_event{}, user_timer,
        connection_end_event{} } );
}
