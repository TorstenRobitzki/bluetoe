#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <bluetoe/link_layer.hpp>

using bluetoe::link_layer::delta_time;
using bluetoe::link_layer::connection_event_events;
using bluetoe::link_layer::peripheral_latency_configuration;
using bluetoe::link_layer::peripheral_latency_configuration_set;
using bluetoe::link_layer::peripheral_latency;

static const delta_time typical_connection_interval = delta_time::msec( 30 );

// the events of a connection event, in the order of the members of connection_event_events
static const connection_event_events no_events;
static const connection_event_events all_events = { true, true, true, true, true, true };
static const connection_event_events all_but_error_events = { true, true, true, true, true, false };
static const connection_event_events unacknowledged_data_events = { true, false, false, false, false, false };
static const connection_event_events last_received_not_empty_events = { false, true, false, false, false, false };
static const connection_event_events last_transmitted_not_empty_events = { false, false, true, false, false, false };
static const connection_event_events last_received_had_more_data_events = { false, false, false, true, false, false };
static const connection_event_events pending_outgoing_data_events = { false, false, false, false, true, false };
static const connection_event_events last_error_events = { false, false, false, false, false, true };

static const std::pair< bool, std::int16_t > no_pending_instant = { false, 0u };

/*
 * The peripheral latency state under test, with the typical connection interval, the radio's
 * answer to a cancelled connection event, and the planned event in one check
 */
template < class Configuration >
struct latency_state : bluetoe::link_layer::details::peripheral_latency_state< Configuration >
{
    latency_state()
    {
        this->reset_connection_state();
    }

    void plan( unsigned latency, const connection_event_events& events = no_events )
    {
        this->plan_next_connection_event( latency, events, typical_connection_interval, no_pending_instant );
    }

    // a plan while a procedure has an instant pending
    void plan_before_instant( unsigned latency, std::int16_t instant )
    {
        this->plan_next_connection_event( latency, no_events, typical_connection_interval, { true, instant } );
    }

    void timeout()
    {
        this->plan_next_connection_event_after_timeout( typical_connection_interval );
    }

    bool reschedule_on_pending_data()
    {
        return bluetoe::link_layer::details::peripheral_latency_state< Configuration >::reschedule_on_pending_data( *this, typical_connection_interval );
    }

    // the channel index and event counter of the planned event, and the time since the last one in intervals
    void check_planned( unsigned channel_index, unsigned event_counter, unsigned intervals ) const
    {
        BOOST_TEST( this->current_channel_index() == channel_index );
        BOOST_TEST( this->connection_event_counter() == event_counter );
        BOOST_TEST( this->time_since_last_event() == intervals * typical_connection_interval );
    }

    bool cancel_radio_event()
    {
        return cancel_radio_event_result;
    }

    bool cancel_radio_event_result = false;
};

using no_peripheral_latency = latency_state< bluetoe::link_layer::peripheral_latency_ignored >;

// In contrast to ignored latency, a latency that really schedules events only at latency anchors
using peripheral_latency_only_at_anchors = latency_state< peripheral_latency_configuration<> >;

using listen_if_unacknowledged_data = latency_state< peripheral_latency_configuration<
    peripheral_latency::listen_if_unacknowledged_data > >;

using listen_if_last_received_not_empty = latency_state< peripheral_latency_configuration<
    peripheral_latency::listen_if_last_received_not_empty > >;

using listen_if_last_transmitted_not_empty = latency_state< peripheral_latency_configuration<
    peripheral_latency::listen_if_last_transmitted_not_empty > >;

using listen_if_last_received_had_more_data = latency_state< peripheral_latency_configuration<
    peripheral_latency::listen_if_last_received_had_more_data > >;

using listen_if_pending_transmit_data = latency_state< peripheral_latency_configuration<
    peripheral_latency::listen_if_pending_transmit_data > >;

using listen_on_multiple_events = latency_state< peripheral_latency_configuration<
    peripheral_latency::listen_if_pending_transmit_data,
    peripheral_latency::listen_if_last_transmitted_not_empty > >;

using config1 = peripheral_latency_configuration<
    peripheral_latency::listen_if_pending_transmit_data,
    peripheral_latency::listen_if_last_transmitted_not_empty >;

using config2 = peripheral_latency_configuration<
    peripheral_latency::listen_if_pending_transmit_data,
    peripheral_latency::listen_if_last_received_not_empty >;

using runtime_configurable = latency_state< peripheral_latency_configuration_set< config1, config2 > >;

/*
 * What every configuration has in common
 */
using all_configurations = boost::mpl::list<
    no_peripheral_latency,
    peripheral_latency_only_at_anchors,
    listen_if_unacknowledged_data,
    listen_if_last_received_not_empty,
    listen_if_last_transmitted_not_empty,
    listen_if_last_received_had_more_data,
    listen_if_pending_transmit_data,
    listen_on_multiple_events,
    runtime_configurable >;

using configurations_with_latency = boost::mpl::list<
    peripheral_latency_only_at_anchors,
    listen_if_unacknowledged_data,
    listen_if_last_received_not_empty,
    listen_if_last_transmitted_not_empty,
    listen_if_last_received_had_more_data,
    listen_if_pending_transmit_data >;

// the configurations that can not cancel a planned connection event
using configurations_without_rescheduling = boost::mpl::list<
    no_peripheral_latency,
    peripheral_latency_only_at_anchors,
    listen_if_unacknowledged_data,
    listen_if_last_received_not_empty,
    listen_if_last_transmitted_not_empty,
    listen_if_last_received_had_more_data >;

using configurations_with_instant_limit = boost::mpl::list<
    peripheral_latency_only_at_anchors,
    listen_on_multiple_events,
    runtime_configurable >;

BOOST_AUTO_TEST_CASE_TEMPLATE( initial_state, State, all_configurations )
{
    State state;

    state.check_planned( 0, 0, 0 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( take_next_event_on_error, State, configurations_with_latency )
{
    State state;

    state.plan( 500, last_error_events );

    state.check_planned( 1, 1, 1 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_disarm_connection_event_support_required, State, configurations_without_rescheduling )
{
    State state;

    BOOST_TEST( state.reschedule_on_pending_data() == false );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( latency_limited_by_pending_instant, State, configurations_with_instant_limit )
{
    State state;

    state.plan_before_instant( 500, 299 );

    state.check_planned( 299u % 37, 299, 299 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( latency_not_limited_by_pending_instant, State, configurations_with_instant_limit )
{
    State state;

    state.plan_before_instant( 100, 299 );

    state.check_planned( 101u % 37, 101, 101 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( latency_limited_by_pending_instant_after_counter_wrap, State, configurations_with_instant_limit )
{
    State state;

    for ( int i = 0; i != 130; ++i )
        state.plan( 500 );

    state.check_planned( 65130u % 37, 65130, 501 );

    state.plan_before_instant( 500, 20 );

    state.check_planned( ( 0x10000 + 20u ) % 37, 20, 0x10000 - 65130u + 20u );
}

BOOST_FIXTURE_TEST_SUITE( no_peripheral_latency_applied, no_peripheral_latency )

    /*
     * Every planned event is placed from the anchor of the last event that happened, which
     * is what the radio reports when a connection event took place.
     */
    BOOST_AUTO_TEST_CASE( the_next_event_is_planned_from_the_anchor )
    {
        const auto anchor = bluetoe::link_layer::abs_time( 0x1000 );

        connection_event_happened( anchor );

        BOOST_TEST( ( last_connection_event_anchor() == anchor ) );
        BOOST_TEST( ( next_connection_event_anchor() == anchor ) );
        BOOST_TEST( time_since_last_event().zero() );

        plan( 0 );

        BOOST_TEST( ( next_connection_event_anchor() == anchor + typical_connection_interval ) );
        BOOST_TEST( ( last_connection_event_anchor() == anchor ) );
        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

    /*
     * A connection event that times out does not move the anchor, so the next event is
     * planned one interval further from the same one.
     */
    BOOST_AUTO_TEST_CASE( a_timeout_leaves_the_anchor_where_it_was )
    {
        const auto anchor = bluetoe::link_layer::abs_time( 0x1000 );

        connection_event_happened( anchor );

        plan( 0 );
        timeout();

        BOOST_TEST( ( last_connection_event_anchor() == anchor ) );
        BOOST_TEST( ( next_connection_event_anchor() == anchor + 2 * typical_connection_interval ) );
        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( channel_index_and_event_counter_are_incremented_by_one )
    {
        // without peripheral latency
        plan( 0 );
        check_planned( 1, 1, 1 );

        // with peripheral latency
        plan( 10 );
        check_planned( 2, 2, 1 );

        // when a timeout happens
        timeout();
        check_planned( 3, 3, 2 );

        timeout();
        check_planned( 4, 4, 3 );

        // with peripheral latency and all sort of events
        plan( 10, all_events );
        check_planned( 5, 5, 1 );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36 )
    {
        for ( int i = 0; i != 36; ++i )
            plan( 0 );

        BOOST_TEST( current_channel_index() == 36u );

        plan( 0 );

        BOOST_TEST( current_channel_index() == 0u );
    }

    BOOST_AUTO_TEST_CASE( event_counter_wraps_after_ffff )
    {
        for ( int i = 0; i != 0xffff; ++i )
            plan( 0 );

        BOOST_TEST( connection_event_counter() == 0xffffu );

        plan( 0 );

        BOOST_TEST( connection_event_counter() == 0u );
    }

    BOOST_AUTO_TEST_CASE( time_since_last_event_is_set_to_interval )
    {
        plan( 0 );
        BOOST_TEST( time_since_last_event() == typical_connection_interval );

        plan( 0 );
        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( time_since_last_event_is_cummulated_after_timeout )
    {
        timeout();
        BOOST_TEST( time_since_last_event() == typical_connection_interval );

        timeout();
        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );

        plan_next_connection_event_after_timeout( 2 * typical_connection_interval );
        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );

        // and reset after a connection event toke place
        plan( 0 );
        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( only_peripheral_latency_applied, peripheral_latency_only_at_anchors )

    static const int latency = 5;

    BOOST_AUTO_TEST_CASE( next_connection_events )
    {
        plan( latency, all_but_error_events );
        check_planned( 6, 6, 6 );

        plan( latency, all_but_error_events );
        check_planned( 12, 12, 6 );

        timeout();
        check_planned( 13, 13, 7 );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36 )
    {
        for ( int i = 0; i != 6; ++i )
            plan( latency, all_but_error_events );

        BOOST_TEST( current_channel_index() == 36u );

        plan( 0 );

        BOOST_TEST( current_channel_index() == 0u );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_within_a_latency )
    {
        for ( int i = 0; i != 5; ++i )
            plan( 7, all_but_error_events );

        BOOST_TEST( current_channel_index() == 3u );

        plan( 7 );

        BOOST_TEST( current_channel_index() == 11u );
    }

    BOOST_AUTO_TEST_CASE( very_large_latency )
    {
        plan( 500, all_but_error_events );

        check_planned( 501u % 37, 501, 501 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( unacknowlaged_data, listen_if_unacknowledged_data )

    static const int latency = 3;

    BOOST_AUTO_TEST_CASE( following_latency_when_no_event_happend )
    {
        plan( latency );

        check_planned( 4, 4, 4 );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_unacknowlaged_data_was_send )
    {
        plan( latency, unacknowledged_data_events );
        check_planned( 1, 1, 1 );

        plan( latency );
        check_planned( 5, 5, 4 );

        timeout();
        check_planned( 6, 6, 5 );

        plan( latency, unacknowledged_data_events );
        check_planned( 7, 7, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( last_received_not_empty, listen_if_last_received_not_empty )

    static const int latency = 100;

    BOOST_AUTO_TEST_CASE( following_latency_when_no_event_happend )
    {
        plan( latency );

        check_planned( 101u % 37, 101, 101 );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_last_received_not_empty )
    {
        plan( latency, last_received_not_empty_events );
        check_planned( 1, 1, 1 );

        plan( latency );
        check_planned( 102u % 37, 102, 101 );

        timeout();
        check_planned( 103u % 37, 103, 102 );

        plan( latency, all_events );
        check_planned( 104u % 37, 104, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( last_transmitted_not_empty, listen_if_last_transmitted_not_empty )

    static const int latency = 1;

    BOOST_AUTO_TEST_CASE( take_next_event_if_last_transmitted_not_empty )
    {
        plan( latency );
        check_planned( 2, 2, 2 );

        plan( latency, last_transmitted_not_empty_events );
        check_planned( 3, 3, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( last_received_had_more_data, listen_if_last_received_had_more_data )

    static const int latency = 2;

    BOOST_AUTO_TEST_CASE( take_next_event_if_last_received_had_more_data_events )
    {
        plan( latency );
        check_planned( 3, 3, 3 );

        plan( latency, last_received_had_more_data_events );
        check_planned( 4, 4, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( pending_transmit_data, listen_if_pending_transmit_data )

    static const int latency = 7;

    BOOST_AUTO_TEST_CASE( following_latency_when_no_pending_data )
    {
        plan( latency );

        check_planned( 8, 8, 8 );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_pending_data_is_present )
    {
        plan( latency );
        check_planned( 8, 8, 8 );

        plan( latency, pending_outgoing_data_events );
        check_planned( 9, 9, 1 );
    }

    BOOST_AUTO_TEST_CASE( rescheduling_not_possible )
    {
        BOOST_TEST( reschedule_on_pending_data() == false );
    }

    BOOST_AUTO_TEST_CASE( initial_connection_event_can_not_be_rescheduled )
    {
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == false );
    }

    BOOST_AUTO_TEST_CASE( half_way_to_the_connection_event )
    {
        // First, the connection event is planned at the 8th connection event
        plan( latency );
        check_planned( 8, 8, 8 );

        // now it's moved to the first one after the anchor
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 1, 1, 1 );
    }

    BOOST_AUTO_TEST_CASE( just_after_setting_up_the_connection_event )
    {
        // First, the connection event is planned at the 8th connection event
        plan( latency );
        check_planned( 8, 8, 8 );

        // now it's moved to the first one after the anchor
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 1, 1, 1 );
    }

    BOOST_AUTO_TEST_CASE( just_after_setting_up_the_connection_event_without_latency )
    {
        // the connection event is planned at the next connection event already
        plan( 0 );
        check_planned( 1, 1, 1 );

        // won't move
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == false );
    }

    BOOST_AUTO_TEST_CASE( new_outgoing_data_directly_after_the_connection_event_was_planned )
    {
        plan( latency );
        check_planned( 8, 8, 8 );

        plan( latency );
        check_planned( 16, 16, 8 );

        // now, new outgoing data became pending
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 9, 9, 1 );
    }

    BOOST_AUTO_TEST_CASE( reschedule_after_connection_event_counter_overflew )
    {
        for ( int i = 0; i != 8192; ++i )
            plan( latency );

        check_planned( 9, 0, 8 );

        // now, new outgoing data became pending
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 2, 0xfff9, 1 );
    }

    BOOST_AUTO_TEST_CASE( reschedule_after_channel_index_overflew )
    {
        for ( int i = 0; i != 5; ++i )
            plan( latency );

        check_planned( 3, 40, 8 );

        // now, new outgoing data became pending
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 33, 33, 1 );
    }

    // see #97 for more context
    BOOST_AUTO_TEST_CASE( reschedule_after_applying_very_large_latency )
    {
        plan( 100 );
        check_planned( 101u % 37, 101, 101 );

        // now, new outgoing data became pending
        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 1, 1, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( combined_connection_event_events, listen_on_multiple_events )

    static const int latency = 1;

    BOOST_AUTO_TEST_CASE( either_event_ends_the_latency )
    {
        plan( latency );
        check_planned( 2, 2, 2 );

        // all events but the ones that are configured to trigger an early connection event, and errors
        connection_event_events all_but;
        all_but.unacknowledged_data = true;
        all_but.last_received_not_empty = true;
        all_but.last_received_had_more_data = true;

        plan( latency, all_but );
        check_planned( 4, 4, 2 );

        plan( latency, pending_outgoing_data_events );
        check_planned( 5, 5, 1 );

        plan( latency, last_transmitted_not_empty_events );
        check_planned( 6, 6, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( switch_behaviour_at_runtime, runtime_configurable )

    static const int latency = 2;

    BOOST_AUTO_TEST_CASE( no_latency )
    {
        plan( 0 );

        check_planned( 1, 1, 1 );
    }

    BOOST_AUTO_TEST_CASE( with_latency )
    {
        plan( latency );

        check_planned( 3, 3, 3 );
    }

    BOOST_AUTO_TEST_CASE( initially_the_first_configuration_is_used )
    {
        // first configuration reacts to the last_transmit_not_empty
        plan( latency, last_transmitted_not_empty_events );
        check_planned( 1, 1, 1 );

        // but does not react to the last_received_not_empty
        plan( latency, last_received_not_empty_events );
        check_planned( 4, 4, 3 );
    }

    BOOST_AUTO_TEST_CASE( switching_to_config1_yields_the_same_results )
    {
        change_peripheral_latency< config1 >();

        // first configuration reacts to the last_transmit_not_empty
        plan( latency, last_transmitted_not_empty_events );
        check_planned( 1, 1, 1 );

        // but does not react to the last_received_not_empty
        plan( latency, last_received_not_empty_events );
        check_planned( 4, 4, 3 );
    }

    BOOST_AUTO_TEST_CASE( use_second_configuration )
    {
        change_peripheral_latency< config2 >();

        // the second configuration does not react to the last_transmit_not_empty
        plan( latency, last_transmitted_not_empty_events );
        check_planned( 3, 3, 3 );

        // but reacts to the last_received_not_empty
        plan( latency, last_received_not_empty_events );
        check_planned( 4, 4, 1 );
    }

    BOOST_AUTO_TEST_CASE( reschedule )
    {
        plan( latency );
        check_planned( 3, 3, 3 );

        cancel_radio_event_result = true;
        BOOST_TEST( reschedule_on_pending_data() == true );

        check_planned( 1, 1, 1 );
    }

BOOST_AUTO_TEST_SUITE_END()
