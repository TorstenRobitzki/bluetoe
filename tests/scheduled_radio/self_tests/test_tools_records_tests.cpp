/**
 * @file radio_records_tests.cpp
 *
 * What a radio test reads from the records of the device under test: the callbacks and calls
 * of one kind, the sequence of callbacks a program caused, and how a sequence is rendered.
 * No instrument, no rig.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/records.hpp"

#include <vector>

using namespace bluetoe::test_rig;

namespace {

    record a_callback( callback_kind kind )
    {
        record result;
        result.kind     = record_kind::callback;
        result.callback = kind;

        return result;
    }

    record a_call( call_kind kind, bool result_of_the_call = true )
    {
        record result;
        result.kind   = record_kind::call;
        result.call   = kind;
        result.result = result_of_the_call;

        return result;
    }

    const std::vector< record > a_run = {
        a_callback( callback_kind::radio_ready ),
        a_callback( adv_timeout ),
        a_call( call_kind::schedule_timer ),
        a_callback( user_timer ),
        a_call( call_kind::cancel_radio_event, false ),
        a_callback( adv_timeout ) };
}

BOOST_AUTO_TEST_CASE( the_callbacks_of_one_kind_come_in_the_order_they_were_recorded )
{
    BOOST_CHECK_EQUAL( callbacks_of( a_run, adv_timeout ).size(), 2u );
    BOOST_CHECK_EQUAL( callbacks_of( a_run, user_timer ).size(), 1u );
    BOOST_CHECK( callbacks_of( a_run, adv_received ).empty() );
}

BOOST_AUTO_TEST_CASE( a_call_is_not_a_callback_and_the_other_way_round )
{
    BOOST_CHECK_EQUAL( calls_of( a_run, call_kind::schedule_timer ).size(), 1u );
    BOOST_CHECK( calls_of( a_run, call_kind::schedule_advertising_event ).empty() );
    BOOST_CHECK( !the_only( calls_of( a_run, call_kind::cancel_radio_event ) ).result );
}

BOOST_AUTO_TEST_CASE( the_one_record_of_a_kind_is_handed_over )
{
    BOOST_CHECK( the_only( callbacks_of( a_run, user_timer ) ).callback == user_timer );
}

// the rig reports it once when the radio came up, before a program was loaded
BOOST_AUTO_TEST_CASE( the_sequence_of_callbacks_leaves_radio_ready_out )
{
    BOOST_CHECK_EQUAL( as_text( a_run, {} ), "adv_timeout, user_timer, adv_timeout" );
}

BOOST_AUTO_TEST_CASE( a_sequence_is_rendered_as_names_separated_by_commas )
{
    BOOST_CHECK_EQUAL( as_text( { adv_timeout, user_timer } ), "adv_timeout, user_timer" );
    BOOST_CHECK_EQUAL( as_text( std::vector< expected_callback >{} ), "" );
    BOOST_CHECK_EQUAL( as_text( callback_kind::connection_end_event ), "connection_end_event" );
}

BOOST_AUTO_TEST_CASE( a_run_that_caused_the_expected_callbacks_passes )
{
    check_callbacks( a_run, { adv_timeout, user_timer, adv_timeout } );
}

BOOST_AUTO_TEST_SUITE( the_flags_of_a_connection_event )

    record an_end_event( const bluetoe::link_layer::connection_event_events& events )
    {
        record result = a_callback( callback_kind::connection_end_event );
        result.events = events;

        return result;
    }

    // unacknowledged, received not empty, transmitted not empty, received MD, pending, error
    const bluetoe::link_layer::connection_event_events pending_data( false, false, true, false, true, false );

    BOOST_AUTO_TEST_CASE( an_expected_end_event_names_the_flags_it_requires_set )
    {
        BOOST_CHECK_EQUAL( as_text( { connection_end_event{} } ), "connection_end_event{}" );
        BOOST_CHECK_EQUAL(
            as_text( { connection_end_event{ .last_transmitted_not_empty = true, .pending_outgoing_data = true } } ),
            "connection_end_event{ last_transmitted_not_empty, pending_outgoing_data }" );
    }

    BOOST_AUTO_TEST_CASE( a_recorded_end_event_shows_its_flags_where_they_are_expected )
    {
        const std::vector< record > run = { an_end_event( pending_data ) };

        BOOST_CHECK_EQUAL( as_text( run, { connection_end_event{} } ),
            "connection_end_event{ last_transmitted_not_empty, pending_outgoing_data }" );
    }

    // an end event in the sequence without flags requires the kind only
    BOOST_AUTO_TEST_CASE( a_recorded_end_event_shows_no_flags_where_none_are_expected )
    {
        const std::vector< record > run = { an_end_event( pending_data ) };

        BOOST_CHECK_EQUAL( as_text( run, { callback_kind::connection_end_event } ), "connection_end_event" );
    }

    BOOST_AUTO_TEST_CASE( flags_expected_past_the_recorded_sequence_are_not_shown_on_it )
    {
        const std::vector< record > run = { a_callback( adv_timeout ), an_end_event( pending_data ) };

        BOOST_CHECK_EQUAL( as_text( run, { adv_timeout } ), "adv_timeout, connection_end_event" );
    }

    BOOST_AUTO_TEST_CASE( an_end_event_with_the_flags_expected_passes )
    {
        const std::vector< record > run = { a_callback( adv_timeout ), an_end_event( pending_data ) };

        check_callbacks( run, { adv_timeout,
            connection_end_event{ .last_transmitted_not_empty = true, .pending_outgoing_data = true } } );
    }

BOOST_AUTO_TEST_SUITE_END()
