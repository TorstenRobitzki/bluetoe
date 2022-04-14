#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer.hpp>

struct no_peripheral_latency : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_ignored
>
{
    no_peripheral_latency()
    {
        reset_connection_state();
    }
};

static const bluetoe::link_layer::delta_time typical_connection_interval =
    bluetoe::link_layer::delta_time::msec( 30 );

static const bluetoe::link_layer::delta_time half_typical_connection_interval =
    bluetoe::link_layer::delta_time::msec( 15 );

static const bluetoe::link_layer::connection_event_events no_events;
static const bluetoe::link_layer::connection_event_events all_events = { true, true, true, true, true, true };
static const bluetoe::link_layer::connection_event_events all_but_error_events = { true, true, true, true, true, false };
static const bluetoe::link_layer::connection_event_events unacknowledged_data_events = { true, false, false, false, false, false };
static const bluetoe::link_layer::connection_event_events last_received_not_empty_events = { false, true, false, false, false, false };
static const bluetoe::link_layer::connection_event_events last_transmitted_not_empty_events = { false, false, true, false, false, false };
static const bluetoe::link_layer::connection_event_events last_received_had_more_data_events = { false, false, false, true, false, false };
static const bluetoe::link_layer::connection_event_events pending_outgoing_data_events = { false, false, false, false, true, false };
static const bluetoe::link_layer::connection_event_events last_error_events = { false, false, false, false, false, true };

static const std::pair< bool, std::int16_t > no_pending_instance = { false, 0u };

BOOST_FIXTURE_TEST_SUITE( no_peripheral_latency_applied, no_peripheral_latency )

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( channel_index_is_incremented_by_one )
    {
        // without peripheral latency
        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );

        // with peripheral latency
        plan_next_connection_event(
            10, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 2u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( current_channel_index() == 3u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( current_channel_index() == 4u );

        // with peripheral latency and all sort of events
        plan_next_connection_event(
            10, all_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 5u );
    }

    BOOST_AUTO_TEST_CASE( event_counter_increments_by_one )
    {
        // without peripheral latency
        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( connection_event_counter() == 1u );

        // with peripheral latency
        plan_next_connection_event(
            10, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( connection_event_counter() == 2u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( connection_event_counter() == 3u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( connection_event_counter() == 4u );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36 )
    {
        for ( int i = 0; i != 36; ++i )
        {
            plan_next_connection_event(
                0, no_events, typical_connection_interval, no_pending_instance );
        }

        BOOST_TEST( current_channel_index() == 36u );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 0u );
    }

    BOOST_AUTO_TEST_CASE( instance_counter_wraps_after_ffff )
    {
        for ( int i = 0; i != 0xffff; ++i )
        {
            plan_next_connection_event(
                0, no_events, typical_connection_interval, no_pending_instance );
        }

        BOOST_TEST( connection_event_counter() == 0xffffu );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( connection_event_counter() == 0u );
    }

    BOOST_AUTO_TEST_CASE( time_since_last_event_is_set_to_interval )
    {
        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( time_since_last_event_is_cummulated_after_timeout )
    {
        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );

        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );

        plan_next_connection_event_after_timeout(
            2 * typical_connection_interval );

        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );

        // and reset after a connection event toke place
        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( no_disarm_connection_event_support_required )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

BOOST_AUTO_TEST_SUITE_END()

// In contrast to ignored latency, a latency that really schedules events only at latency anchors
struct peripheral_latency_only_at_anchors : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<>
>
{
    peripheral_latency_only_at_anchors()
    {
        reset_connection_state();
    }
};

BOOST_FIXTURE_TEST_SUITE( only_peripheral_latency_applied, peripheral_latency_only_at_anchors )

    static const int latency = 5;

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( next_connection_events )
    {
        plan_next_connection_event(
            latency, all_but_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 6u );
        BOOST_TEST( connection_event_counter() == 6u );
        BOOST_TEST( time_since_last_event() == 6 * typical_connection_interval );

        plan_next_connection_event(
            latency, all_but_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 12u );
        BOOST_TEST( connection_event_counter() == 12u );
        BOOST_TEST( time_since_last_event() == 6 * typical_connection_interval );

        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( current_channel_index() == 13u );
        BOOST_TEST( connection_event_counter() == 13u );
        BOOST_TEST( time_since_last_event() == 7 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36 )
    {
        for ( int i = 0; i != 6; ++i )
        {
            plan_next_connection_event(
                latency, all_but_error_events, typical_connection_interval, no_pending_instance );
        }

        BOOST_TEST( current_channel_index() == 36u );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 0u );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36_II )
    {
        for ( int i = 0; i != 5; ++i )
        {
            plan_next_connection_event(
                7, all_but_error_events, typical_connection_interval, no_pending_instance );
        }

        BOOST_TEST( current_channel_index() == 3u );

        plan_next_connection_event(
            7, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 11u );
    }

    BOOST_AUTO_TEST_CASE( very_large_latency )
    {
        plan_next_connection_event(
            500, all_but_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 501u % 37);
        BOOST_TEST( connection_event_counter() == 501u );
        BOOST_TEST( time_since_last_event() == 501 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( no_disarm_connection_event_support_required )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_on_error )
    {
        plan_next_connection_event(
            500, last_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_limited_by_pending_instance )
    {
        plan_next_connection_event(
            500, no_events, typical_connection_interval, { true, 299u } );

        BOOST_TEST( current_channel_index() == 299u % 37 );
        BOOST_TEST( connection_event_counter() == 299u );
        BOOST_TEST( time_since_last_event() == 299u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_not_limited_by_pending_instance )
    {
        plan_next_connection_event(
            100, no_events, typical_connection_interval, { true, 299u } );

        BOOST_TEST( current_channel_index() == 101u % 37 );
        BOOST_TEST( connection_event_counter() == 101u );
        BOOST_TEST( time_since_last_event() == 101u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_limited_by_pending_instance_after_counter_wrap )
    {
        for ( int i = 0; i != 130; ++i )
            plan_next_connection_event(
                500, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 65130u % 37 );
        BOOST_TEST( connection_event_counter() == 65130u );
        BOOST_TEST( time_since_last_event() == 501u * typical_connection_interval );

        plan_next_connection_event(
            500, no_events, typical_connection_interval, { true, 20 } );

        BOOST_TEST( current_channel_index() == ( 0x10000 + 20u ) % 37 );
        BOOST_TEST( connection_event_counter() == 20u );
        BOOST_TEST( time_since_last_event() == ( 0x10000 - 65130u + 20u ) * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct listen_if_unacknowledged_data : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<
        bluetoe::link_layer::peripheral_latency::listen_if_unacknowledged_data
    >
>
{
    listen_if_unacknowledged_data()
    {
        reset_connection_state();
    }
};

BOOST_FIXTURE_TEST_SUITE( unacknowlaged_data, listen_if_unacknowledged_data )

    static const int latency = 3;

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( following_latency_when_no_event_happend )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_unacknowlaged_data_was_send )
    {
        plan_next_connection_event(
            latency, unacknowledged_data_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );

        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 5u );
        BOOST_TEST( connection_event_counter() == 5u );
        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );

        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( current_channel_index() == 6u );
        BOOST_TEST( connection_event_counter() == 6u );
        BOOST_TEST( time_since_last_event() == 5 * typical_connection_interval );

        plan_next_connection_event(
            latency, unacknowledged_data_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 7u );
        BOOST_TEST( connection_event_counter() == 7u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( no_disarm_connection_event_support_required )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_on_error )
    {
        plan_next_connection_event(
            500, last_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct listen_if_last_received_not_empty : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<
        bluetoe::link_layer::peripheral_latency::listen_if_last_received_not_empty
    >
>
{
    listen_if_last_received_not_empty()
    {
        reset_connection_state();
    }
};

BOOST_FIXTURE_TEST_SUITE( last_received_not_empty, listen_if_last_received_not_empty )

    static const int latency = 100;

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( following_latency_when_no_event_happend )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 101u % 37u );
        BOOST_TEST( connection_event_counter() == 101u );
        BOOST_TEST( time_since_last_event() == 101u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_unacknowlaged_data_was_send )
    {
        plan_next_connection_event(
            latency, last_received_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );

        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 102u % 37u );
        BOOST_TEST( connection_event_counter() == 102u );
        BOOST_TEST( time_since_last_event() == 101 * typical_connection_interval );

        plan_next_connection_event_after_timeout(
            typical_connection_interval );

        BOOST_TEST( current_channel_index() == 103u % 37u );
        BOOST_TEST( connection_event_counter() == 103u );
        BOOST_TEST( time_since_last_event() == 102 * typical_connection_interval );

        plan_next_connection_event(
            latency, all_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 104u % 37u );
        BOOST_TEST( connection_event_counter() == 104 );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( no_disarm_connection_event_support_required )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_on_error )
    {
        plan_next_connection_event(
            500, last_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct listen_if_last_transmitted_not_empty : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<
        bluetoe::link_layer::peripheral_latency::listen_if_last_transmitted_not_empty
    >
>
{
    listen_if_last_transmitted_not_empty()
    {
        reset_connection_state();
    }
};

BOOST_FIXTURE_TEST_SUITE( last_transmitted_not_empty, listen_if_last_transmitted_not_empty )

    static const int latency = 1;

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_last_transmitted_not_empty )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 2u );
        BOOST_TEST( connection_event_counter() == 2u );
        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );

        plan_next_connection_event(
            latency, last_transmitted_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 3u );
        BOOST_TEST( connection_event_counter() == 3u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( no_disarm_connection_event_support_required )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_on_error )
    {
        plan_next_connection_event(
            500, last_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct listen_if_last_received_had_more_data : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<
        bluetoe::link_layer::peripheral_latency::listen_if_last_received_had_more_data
    >
>
{
    listen_if_last_received_had_more_data()
    {
        reset_connection_state();
    }
};

BOOST_FIXTURE_TEST_SUITE( last_received_had_more_data, listen_if_last_received_had_more_data )

    static const int latency = 2;

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_last_received_had_more_data_events )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 3u );
        BOOST_TEST( connection_event_counter() == 3u );
        BOOST_TEST( time_since_last_event() == 3 * typical_connection_interval );

        plan_next_connection_event(
            latency, last_received_had_more_data_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( no_disarm_connection_event_support_required )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_on_error )
    {
        plan_next_connection_event(
            500, last_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct listen_if_pending_transmit_data : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<
        bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data
    >
>
{
    listen_if_pending_transmit_data()
        : disarm_connection_event_result( false, bluetoe::link_layer::delta_time() )
    {
        reset_connection_state();
    }

    std::pair< bool, bluetoe::link_layer::delta_time > disarm_connection_event()
    {
        return disarm_connection_event_result;
    }

    std::pair< bool, bluetoe::link_layer::delta_time > disarm_connection_event_result;
};

BOOST_FIXTURE_TEST_SUITE( pending_transmit_data, listen_if_pending_transmit_data )

    static const int latency = 7;

    BOOST_AUTO_TEST_CASE( initial_state )
    {
        BOOST_TEST( current_channel_index() == 0u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event().zero() );
    }

    BOOST_AUTO_TEST_CASE( following_latency_when_no_pending_data )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 8u );
        BOOST_TEST( connection_event_counter() == 8u );
        BOOST_TEST( time_since_last_event() == 8u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_if_pending_data_is_present )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 8u );
        BOOST_TEST( connection_event_counter() == 8u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        plan_next_connection_event(
            latency, pending_outgoing_data_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 9u );
        BOOST_TEST( connection_event_counter() == 9u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( rescheduling_not_possible )
    {
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( initial_connection_event_can_not_be_rescheduled )
    {
        disarm_connection_event_result = { true, half_typical_connection_interval };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( half_way_to_the_connection_event )
    {
        // First, the connection event is planned at the 8th connection event
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 8u );
        BOOST_TEST( connection_event_counter() == 8u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        // now it's moved to 4th
        disarm_connection_event_result = { true, 3 * typical_connection_interval + half_typical_connection_interval };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == true );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( just_after_setting_up_the_connection_event )
    {
        // First, the connection event is planned at the 8th connection event
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 8u );
        BOOST_TEST( connection_event_counter() == 8u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        // now it's moved to 4th
        disarm_connection_event_result = { true, bluetoe::link_layer::delta_time() };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == true );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( just_after_setting_up_the_connection_event_without_latency )
    {
        // First, the connection event is planned at the 8th connection event
        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );

        // won't move
        disarm_connection_event_result = { true, bluetoe::link_layer::delta_time() };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == false );
    }

    BOOST_AUTO_TEST_CASE( new_outgoing_data_directly_after_the_connection_event_was_planned )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 8u );
        BOOST_TEST( connection_event_counter() == 8u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 16u );
        BOOST_TEST( connection_event_counter() == 16u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        // now, new outgoing data became pending
        disarm_connection_event_result = { true, half_typical_connection_interval };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == true );

        BOOST_TEST( current_channel_index() == 9u );
        BOOST_TEST( connection_event_counter() == 9u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( reschedule_after_connection_instance_overflew )
    {
        for ( int i = 0; i != 8192; ++i )
            plan_next_connection_event(
                latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 9u );
        BOOST_TEST( connection_event_counter() == 0u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        // now, new outgoing data became pending
        disarm_connection_event_result = { true, half_typical_connection_interval };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == true );

        BOOST_TEST( current_channel_index() == 2u );
        BOOST_TEST( connection_event_counter() == 0xfff9u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( reschedule_after_channel_index_overflew )
    {
        for ( int i = 0; i != 5; ++i )
            plan_next_connection_event(
                latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 3u );
        BOOST_TEST( connection_event_counter() == 40u );
        BOOST_TEST( time_since_last_event() == 8 * typical_connection_interval );

        // now, new outgoing data became pending
        disarm_connection_event_result = { true, 3 * typical_connection_interval + half_typical_connection_interval };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == true );

        BOOST_TEST( current_channel_index() == 36u );
        BOOST_TEST( connection_event_counter() == 36u );
        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( take_next_event_on_error )
    {
        plan_next_connection_event(
            500, last_error_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct listen_on_multiple_events : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration<
        bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
        bluetoe::link_layer::peripheral_latency::listen_if_last_transmitted_not_empty
    >
>
{
    listen_on_multiple_events()
        : disarm_connection_event_result( false, bluetoe::link_layer::delta_time() )
    {
        reset_connection_state();

        // All events, but the ones that are configured to trigger an early connection event
        // and errors
        all_but.unacknowledged_data = true;
        all_but.last_received_not_empty = true;
        all_but.last_received_had_more_data = true;
    }

    std::pair< bool, bluetoe::link_layer::delta_time > disarm_connection_event()
    {
        return disarm_connection_event_result;
    }

    std::pair< bool, bluetoe::link_layer::delta_time > disarm_connection_event_result;
    bluetoe::link_layer::connection_event_events all_but;
};

BOOST_FIXTURE_TEST_SUITE( combined_connection_event_events, listen_on_multiple_events )

    static const int latency = 1;

    BOOST_AUTO_TEST_CASE( test_for_both_events )
    {
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 2u );
        BOOST_TEST( connection_event_counter() == 2u );
        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );

        plan_next_connection_event(
            latency, all_but, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );

        plan_next_connection_event(
            latency, pending_outgoing_data_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 5u );
        BOOST_TEST( connection_event_counter() == 5u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );

        plan_next_connection_event(
            latency, last_transmitted_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 6u );
        BOOST_TEST( connection_event_counter() == 6u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_limited_by_pending_instance )
    {
        plan_next_connection_event(
            500, no_events, typical_connection_interval, { true, 299u } );

        BOOST_TEST( current_channel_index() == 299u % 37 );
        BOOST_TEST( connection_event_counter() == 299u );
        BOOST_TEST( time_since_last_event() == 299u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_not_limited_by_pending_instance )
    {
        plan_next_connection_event(
            100, no_events, typical_connection_interval, { true, 299u } );

        BOOST_TEST( current_channel_index() == 101u % 37 );
        BOOST_TEST( connection_event_counter() == 101u );
        BOOST_TEST( time_since_last_event() == 101u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_limited_by_pending_instance_after_counter_wrap )
    {
        for ( int i = 0; i != 130; ++i )
            plan_next_connection_event(
                500, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 65130u % 37 );
        BOOST_TEST( connection_event_counter() == 65130u );
        BOOST_TEST( time_since_last_event() == 501u * typical_connection_interval );

        plan_next_connection_event(
            500, no_events, typical_connection_interval, { true, 20 } );

        BOOST_TEST( current_channel_index() == ( 0x10000 + 20u ) % 37 );
        BOOST_TEST( connection_event_counter() == 20u );
        BOOST_TEST( time_since_last_event() == ( 0x10000 - 65130u + 20u ) * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

struct runtime_configurable : bluetoe::link_layer::details::peripheral_latency_state<
    bluetoe::link_layer::peripheral_latency_configuration_set<
        bluetoe::link_layer::peripheral_latency_configuration<
            bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
            bluetoe::link_layer::peripheral_latency::listen_if_last_transmitted_not_empty
        >,
        bluetoe::link_layer::peripheral_latency_configuration<
            bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
            bluetoe::link_layer::peripheral_latency::listen_if_last_received_not_empty
        >
    >
>
{
    using config1 = bluetoe::link_layer::peripheral_latency_configuration<
            bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
            bluetoe::link_layer::peripheral_latency::listen_if_last_transmitted_not_empty
        >;

    using config2 = bluetoe::link_layer::peripheral_latency_configuration<
            bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
            bluetoe::link_layer::peripheral_latency::listen_if_last_received_not_empty
        >;

    runtime_configurable()
    {
        reset_connection_state();
    }

    std::pair< bool, bluetoe::link_layer::delta_time > disarm_connection_event()
    {
        return disarm_connection_event_result;
    }

    std::pair< bool, bluetoe::link_layer::delta_time > disarm_connection_event_result;
};

BOOST_FIXTURE_TEST_SUITE( switch_behaviour_at_runtime, runtime_configurable )

    static const int latency = 2;

    BOOST_AUTO_TEST_CASE( no_latency )
    {
        // a first connection event, without latency or any event
        plan_next_connection_event(
            0, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( with_latency )
    {
        // a first connection event, without latency or any event
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 3u );
        BOOST_TEST( connection_event_counter() == 3u );
        BOOST_TEST( time_since_last_event() == 3 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( intiatially_the_first_configuration_is_used )
    {
        // first configuration reacts to the last_transmit_not_empty
        plan_next_connection_event(
            latency, last_transmitted_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );

        // but does not react to the last_received_not_empty
        plan_next_connection_event(
            latency, last_received_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 3 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( switching_to_config1_yields_the_same_results )
    {
        change_peripheral_latency< config1 >();

        // first configuration reacts to the last_transmit_not_empty
        plan_next_connection_event(
            latency, last_transmitted_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );

        // but does not react to the last_received_not_empty
        plan_next_connection_event(
            latency, last_received_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 3 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( use_second_configuration )
    {
        change_peripheral_latency< config2 >();

        // first configuration reacts to the last_transmit_not_empty
        plan_next_connection_event(
            latency, last_transmitted_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 3u );
        BOOST_TEST( connection_event_counter() == 3u );
        BOOST_TEST( time_since_last_event() == 3 * typical_connection_interval );

        // but does not react to the last_received_not_empty
        plan_next_connection_event(
            latency, last_received_not_empty_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 4u );
        BOOST_TEST( connection_event_counter() == 4u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( reschedule )
    {
        // First, the connection event is planned at the 8th connection event
        plan_next_connection_event(
            latency, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 3u );
        BOOST_TEST( connection_event_counter() == 3u );
        BOOST_TEST( time_since_last_event() == 3 * typical_connection_interval );

        disarm_connection_event_result = { true, half_typical_connection_interval };
        BOOST_TEST( reschedule_on_pending_data( *this, typical_connection_interval ) == true );

        BOOST_TEST( current_channel_index() == 1u );
        BOOST_TEST( connection_event_counter() == 1u );
        BOOST_TEST( time_since_last_event() == 1 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_limited_by_pending_instance )
    {
        plan_next_connection_event(
            500, no_events, typical_connection_interval, { true, 299u } );

        BOOST_TEST( current_channel_index() == 299u % 37 );
        BOOST_TEST( connection_event_counter() == 299u );
        BOOST_TEST( time_since_last_event() == 299u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_not_limited_by_pending_instance )
    {
        plan_next_connection_event(
            100, no_events, typical_connection_interval, { true, 299u } );

        BOOST_TEST( current_channel_index() == 101u % 37 );
        BOOST_TEST( connection_event_counter() == 101u );
        BOOST_TEST( time_since_last_event() == 101u * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( latency_limited_by_pending_instance_after_counter_wrap )
    {
        for ( int i = 0; i != 130; ++i )
            plan_next_connection_event(
                500, no_events, typical_connection_interval, no_pending_instance );

        BOOST_TEST( current_channel_index() == 65130u % 37 );
        BOOST_TEST( connection_event_counter() == 65130u );
        BOOST_TEST( time_since_last_event() == 501u * typical_connection_interval );

        plan_next_connection_event(
            500, no_events, typical_connection_interval, { true, 20 } );

        BOOST_TEST( current_channel_index() == ( 0x10000 + 20u ) % 37 );
        BOOST_TEST( connection_event_counter() == 20u );
        BOOST_TEST( time_since_last_event() == ( 0x10000 - 65130u + 20u ) * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()
