#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer.hpp>

struct no_peripheral_latency : bluetoe::link_layer::details::connection_state<
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

static const bluetoe::link_layer::connection_event_events no_events;
static const bluetoe::link_layer::connection_event_events all_events = { true, true, true, true };

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
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 1u );

        // with peripheral latency
        plan_next_connection_event(
            10, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 2u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            0, typical_connection_interval );

        BOOST_TEST( current_channel_index() == 3u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            42, typical_connection_interval );

        BOOST_TEST( current_channel_index() == 4u );
    }

    BOOST_AUTO_TEST_CASE( event_counter_increments_by_one )
    {
        // without peripheral latency
        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( connection_event_counter() == 1u );

        // with peripheral latency
        plan_next_connection_event(
            10, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( connection_event_counter() == 2u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            0, typical_connection_interval );

        BOOST_TEST( connection_event_counter() == 3u );

        // when a timeout happens, without peripheral latency
        plan_next_connection_event_after_timeout(
            42, typical_connection_interval );

        BOOST_TEST( connection_event_counter() == 4u );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36 )
    {
        for ( int i = 0; i != 36; ++i )
        {
            plan_next_connection_event(
                0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );
        }

        BOOST_TEST( current_channel_index() == 36u );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 0u );
    }

    BOOST_AUTO_TEST_CASE( instance_counter_wraps_after_ffff )
    {
        for ( int i = 0; i != 0xffff; ++i )
        {
            plan_next_connection_event(
                0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );
        }

        BOOST_TEST( connection_event_counter() == 0xffffu );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( connection_event_counter() == 0u );
    }

    BOOST_AUTO_TEST_CASE( time_since_last_event_is_set_to_interval )
    {
        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( time_since_last_event_is_cummulated_after_timeout )
    {
        plan_next_connection_event_after_timeout(
            42, typical_connection_interval );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );

        plan_next_connection_event_after_timeout(
            3, typical_connection_interval );

        BOOST_TEST( time_since_last_event() == 2 * typical_connection_interval );

        plan_next_connection_event_after_timeout(
            3, 2 * typical_connection_interval );

        BOOST_TEST( time_since_last_event() == 4 * typical_connection_interval );

        // and reset after a connection event toke place
        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( time_since_last_event() == typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()

// In contrast to ignored latency, a latency that really schedules events only at latency anchors
struct peripheral_latency_only_at_anchors : bluetoe::link_layer::details::connection_state<
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
            latency, all_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 6u );
        BOOST_TEST( connection_event_counter() == 6u );
        BOOST_TEST( time_since_last_event() == 6 * typical_connection_interval );

        plan_next_connection_event(
            latency, all_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 12u );
        BOOST_TEST( connection_event_counter() == 12u );
        BOOST_TEST( time_since_last_event() == 12 * typical_connection_interval );

        plan_next_connection_event_after_timeout(
            latency, typical_connection_interval );

        BOOST_TEST( current_channel_index() == 18u );
        BOOST_TEST( connection_event_counter() == 18u );
        BOOST_TEST( time_since_last_event() == 6 * typical_connection_interval );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36 )
    {
        for ( int i = 0; i != 6; ++i )
        {
            plan_next_connection_event(
                latency, all_events, typical_connection_interval, bluetoe::link_layer::delta_time() );
        }

        BOOST_TEST( current_channel_index() == 36u );

        plan_next_connection_event(
            0, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 0u );
    }

    BOOST_AUTO_TEST_CASE( channel_index_wraps_after_36_II )
    {
        for ( int i = 0; i != 5; ++i )
        {
            plan_next_connection_event(
                7, all_events, typical_connection_interval, bluetoe::link_layer::delta_time() );
        }

        BOOST_TEST( current_channel_index() == 3u );

        plan_next_connection_event(
            7, no_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 11u );
    }

    BOOST_AUTO_TEST_CASE( very_large_latency )
    {
        plan_next_connection_event(
            500, all_events, typical_connection_interval, bluetoe::link_layer::delta_time() );

        BOOST_TEST( current_channel_index() == 501u % 37);
        BOOST_TEST( connection_event_counter() == 501u );
        BOOST_TEST( time_since_last_event() == 501 * typical_connection_interval );
    }

BOOST_AUTO_TEST_SUITE_END()
