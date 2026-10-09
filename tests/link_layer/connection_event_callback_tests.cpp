#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/connection_event_callback.hpp>

#include "connected.hpp"

#include <sstream>
#include <chrono>

using namespace test;
using bluetoe::link_layer::delta_time;
using bluetoe::link_layer::abs_time;

using namespace std::literals::chrono_literals;

struct callbacks_t {

    struct connection {
        int value = 0;
    };

    unsigned ll_synchronized_callback( unsigned instant, connection& con )
    {
        connection_values.push_back( con.value );
        ++con.value;
        instants.push_back( instant );

        if ( !planned_latency.empty() )
        {
            const auto result = planned_latency.front();
            planned_latency.erase( planned_latency.begin() );

            return result;
        }

        return 0;
    }

    std::vector< unsigned > planned_latency;
    std::vector< int >      connection_values;
    std::vector< unsigned > instants;

} callbacks;

namespace {
    template < class T >
    T take( const T& c, std::size_t n )
    {
        return T{ c.begin(), std::next(c.begin(), std::min( n, c.size() ) ) };
    }

    // the LL_TERMINATE_IND with the reason: remote user terminated connection
    const auto remote_user_terminated = ll_terminate_ind( 0x13 );

    // a connection update to a 10ms interval at instant 8
    const test::connection_update update_to_10ms_interval = {
        .window_size    = 7,
        .window_offset  = 8,
        .interval       = 8,
        .latency        = 0,
        .timeout        = 100,
        .instant        = 8
    };
}

/*
 * Set of test that should not compile.
 */
BOOST_AUTO_TEST_SUITE( tests_that_should_not_compile )

#if 0
BOOST_AUTO_TEST_CASE( no_support_by_hardware )
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_without_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 1000, -100, 100 >
     >;

     gatt g;
}
#endif

#if 0
BOOST_AUTO_TEST_CASE( MaximumPeriodUS_less_or_equal_minimum_interval )
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_with_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 8000, -100, 100 >,
        bluetoe::link_layer::check_synchronized_connection_event_callback
     >;

     gatt g;
}
#endif

#if 0
BOOST_AUTO_TEST_CASE( PhaseShiftUS_is_negative )
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_with_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 2500, 100, 100 >,
        bluetoe::link_layer::check_synchronized_connection_event_callback
     >;

     gatt g;
}
#endif

#if 0
BOOST_AUTO_TEST_CASE( PhaseShiftGreaterThan_SetupTime )
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_with_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 2500, -50, 100 >,
        bluetoe::link_layer::check_synchronized_connection_event_callback
     >;

     gatt g;
}
#endif

#if 0
BOOST_AUTO_TEST_CASE( PhaseShiftGreaterThanRuntimerAndSetuptime)
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_with_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 2500, -300, 350 >,
        bluetoe::link_layer::check_synchronized_connection_event_callback
     >;

     gatt g;
}
#endif

#if 0
BOOST_AUTO_TEST_CASE( ShiftExceedsPeriod )
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_with_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 200, -300, 100 >,
        bluetoe::link_layer::check_synchronized_connection_event_callback
     >;

     gatt g;
}
#endif

#if 0
BOOST_AUTO_TEST_CASE( MaximumExecutionTimeLessThanHalfOfPeriod )
{
    using gatt = unconnected_base_t<
        test::small_temperature_service,
        test::radio_with_user_timer,
        bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, 1000, -700, 600 >,
        bluetoe::link_layer::check_synchronized_connection_event_callback
     >;

     gatt g;
}
#endif

BOOST_AUTO_TEST_SUITE_END()

template < unsigned MinimumPeriodUS, int PhaseShiftUS, unsigned MaximumExecutionTimeUS = 0 >
struct unconnected_server : unconnected_base_t<
    test::small_temperature_service,
    test::radio_with_user_timer,
    bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, MinimumPeriodUS, PhaseShiftUS, MaximumExecutionTimeUS >
 >
 {
    unconnected_server()
    {
        callbacks = callbacks_t();
    }

    // the time, the n's event happend
    std::chrono::microseconds ev( unsigned n )
    {
        return std::chrono::microseconds(this->connection_events().at( n ).when.data());
    }

    /*
     * The delays of the scheduled user timers from the given one on, in milliseconds
     * before the phase shift is applied
     */
    void check_user_timers( std::size_t index, std::initializer_list< std::chrono::microseconds > times ) const
    {
        const auto timers = this->scheduled_user_timers();

        BOOST_REQUIRE_GE( timers.size(), index + times.size() );

        for ( const auto timer : times )
        {
            BOOST_TEST_CONTEXT( "user timer " << index )
            {
                BOOST_CHECK_EQUAL(
                    std::chrono::microseconds((timers[ index ].when - abs_time()).usec()).count(), timer.count() );
            }

            ++index;
        }
    }

    void check_user_timers( std::initializer_list< std::chrono::microseconds > delays_ms ) const
    {
        check_user_timers( 0, delays_ms );
    }

    // the instants the callback was called with, from the first call on
    void check_instants( std::initializer_list< unsigned > expected ) const
    {
        BOOST_TEST(
            take( callbacks.instants, expected.size() ) == expected,
            boost::test_tools::per_element() );
    }

    // the values of the connection object the callback saw, from the first call on
    void check_connection_values( std::initializer_list< int > expected ) const
    {
        BOOST_TEST(
            take( callbacks.connection_values, expected.size() ) == expected,
            boost::test_tools::per_element() );
    }
 };

using server_7ms_minus_100us  = unconnected_server< 7000, -100 >;
using server_7ms_plus_100us   = unconnected_server< 7000, 100 >;
using server_15ms_minus_100us = unconnected_server< 15000, -100 >;

template < unsigned MinimumPeriodUS, int PhaseShiftUS, unsigned MaximumExecutionTimeUS = 0 >
class fixture :
    public bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, MinimumPeriodUS, PhaseShiftUS, MaximumExecutionTimeUS >
        ::template impl< fixture< MinimumPeriodUS, PhaseShiftUS, MaximumExecutionTimeUS > >
{
public:
    fixture()
    {
        callbacks = callbacks_t();
    }

    struct scheduled_timer {
        abs_time    when;
        delta_time  maximum_execution_time;
    };

    static constexpr bool hardware_supports_synchronized_user_timer = true;

    bool schedule_synchronized_user_timer( abs_time when, delta_time maximum_execution_time )
    {
        BOOST_REQUIRE( !timer_running_ );

        if ( when.is_in_near_past( now_ ) )
            return false;

        timer_running_ = true;

        timers_.emplace_back( when, maximum_execution_time );
        to_be_checked_.emplace_back( when, maximum_execution_time );

        return true;
    }

    bool cancel_synchronized_user_timer()
    {
        const bool result = timer_running_;
        timer_running_ = false;

        return result;
    }

    void fire_timer( abs_time now )
    {
        BOOST_REQUIRE( timer_running_ );
        timer_running_ = false;

        BOOST_REQUIRE(!timers_.empty());
        const auto when = timers_.back().when;

        BOOST_REQUIRE( now_.is_in_near_past_or_now( when ) );
        now_ = when;

        BOOST_REQUIRE_EQUAL( now_, now );
        this->synchronized_connection_event_callback_timeout( now_ );
    }

    void fire_timer()
    {
        BOOST_REQUIRE(!timers_.empty());
        fire_timer( timers_.back().when );
    }

    void fire_timers_until( abs_time until )
    {
        BOOST_REQUIRE( now_.is_in_near_past( until ) );

        while ( !timers_.empty() && timers_.back().when.is_in_near_past_or_now( until ) && timer_running_ )
            fire_timer();

        now_ = until;
    }

    void check_user_timers( abs_time start, std::initializer_list< delta_time > invocations )
    {
        BOOST_REQUIRE_EQUAL( to_be_checked_.size(), invocations.size() );

        std::size_t index = 0;
        for ( const auto invoke : invocations )
        {
            BOOST_TEST_CONTEXT( "user timer " << index )
            {
                BOOST_CHECK_EQUAL( start + invoke, to_be_checked_[ index ].when );
            }

            ++index;
        }

        to_be_checked_.clear();
    }

    // the instants the callback was called with, from the first call on
    void check_instants( std::initializer_list< unsigned > expected ) const
    {
        BOOST_TEST(
            take( callbacks.instants, expected.size() ) == expected,
            boost::test_tools::per_element() );
    }

    // the values of the connection object the callback saw, from the first call on
    void check_connection_values( std::initializer_list< int > expected ) const
    {
        BOOST_TEST(
            take( callbacks.connection_values, expected.size() ) == expected,
            boost::test_tools::per_element() );
    }

    void check_execution_times( std::initializer_list< delta_time > expected ) const
    {
        BOOST_REQUIRE_EQUAL( timers_.size(), expected.size() );

        std::size_t index = 0;
        for ( const auto time : expected )
        {
            BOOST_TEST_CONTEXT( "execution time " << index )
            {
                BOOST_CHECK_EQUAL( time, timers_[ index ].maximum_execution_time );
            }

            ++index;
        }
    }

    void at(abs_time t)
    {
        BOOST_REQUIRE( now_.is_in_near_past( t ) );
        now_ = t;
    }

private:
    bool timer_running_ = false;

    std::vector< scheduled_timer > timers_;
    std::vector< scheduled_timer > to_be_checked_;
    abs_time now_;
};

static const abs_time t0 = abs_time() + 100ms;

using fixture_7ms_minus_100us = fixture< 7000, -100 >;
using fixture_7ms_plus_100us  = fixture< 7000,  100, 5 >;

// this test assumes, that all events are exactly on time and reported at the time, the connection
// event starts.
BOOST_FIXTURE_TEST_CASE( callback_called_with_correct_period_and_phase, fixture_7ms_minus_100us )
{
    // effective period is 6ms (5*6ms == 30ms; 6ms < 7ms)
    const auto interval = 30ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );
    check_user_timers( t0, { 1 * 6ms - 100us } );

    fire_timer( t0 + 1 * 6ms - 100us );
    check_user_timers( t0, { 2 * 6ms - 100us } );

    fire_timer();
    check_user_timers( t0, { 3 * 6ms - 100us } );

    fire_timer();
    check_user_timers( t0, { 4 * 6ms - 100us } );

    fire_timer();
    check_user_timers( t0, { 5 * 6ms - 100us } );

    fire_timer();
    check_user_timers( t0, { 6 * 6ms - 100us } );

    at( t0 + 5 * 6ms );
    synchronized_connection_event_callback_new_anchor( t0 + interval, interval );

    fire_timer();
    check_user_timers( t0, { 7 * 6ms - 100us } );

    check_execution_times( { 0us, 0us, 0us, 0us, 0us, 0us, 0us } );
}

// this test assumes, that all events are exactly on time and reported at the time, the connection
// event starts.
BOOST_FIXTURE_TEST_CASE( callback_called_with_correct_period_and_positive_phase, fixture_7ms_plus_100us )
{
    // effective period is 6ms (5*6ms == 30ms; 6ms < 7ms)
    const auto interval = 30ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );
    check_user_timers( t0, { 1 * 6ms + 100us } );

    fire_timer();
    check_user_timers( t0, { 2 * 6ms + 100us } );

    fire_timer();
    check_user_timers( t0, { 3 * 6ms + 100us } );

    fire_timer();
    check_user_timers( t0, { 4 * 6ms + 100us } );

    fire_timer( t0 + 4 * 6ms + 100us );
    check_user_timers( t0, { 5 * 6ms + 100us } );

    at( t0 + 5 * 6ms );
    synchronized_connection_event_callback_new_anchor( t0 + interval, interval );

    fire_timer();
    check_user_timers( t0, { 6 * 6ms + 100us } );

    fire_timer();
    check_user_timers( t0, { 7 * 6ms + 100us } );

    check_execution_times( { 5us, 5us, 5us, 5us, 5us, 5us, 5us } );
}

BOOST_FIXTURE_TEST_CASE( timer_after_the_anchor_fires_before_the_event_is_reported, fixture_7ms_plus_100us )
{
    // effective period is 5ms (2*5ms == 10ms; 5ms < 7ms)
    const auto interval = 10ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );
    check_user_timers( t0, { 1 * 5ms + 100us } );

    fire_timer();
    check_user_timers( t0, { 2 * 5ms + 100us } );

    // the timer 100us after the anchor fires before the event, which starts at the anchor,
    // is reported 200us later due to the length of the event
    fire_timer( t0 + 2 * 5ms + 100us );
    check_user_timers( t0, { 3 * 5ms + 100us } );

    at( t0 + 2 * 5ms + 200us );
    synchronized_connection_event_callback_new_anchor( t0 + interval, interval );

    fire_timer();
    check_user_timers( t0, { 4 * 5ms + 100us } );

    // The connection interval at t0 + 2 * interval was timing out
    // still, the timer is going
    fire_timer( t0 + 4 * 5ms + 100us );
    check_user_timers( t0, { 5 * 5ms + 100us } );
}

// make sure, that when an event happend not in time, but with a drift within
// the normal window, that the user timer is synchronized to the events
BOOST_FIXTURE_TEST_CASE( grid_follows_a_drifted_anchor, fixture_7ms_plus_100us )
{
    const auto t0_p = t0 + 20us;
    const auto t0_m = t0 - 15us;

    // effective period is 5ms (2*5ms == 10ms; 5ms < 7ms)
    const auto interval = 10ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );
    check_user_timers( t0, { 1 * 5ms + 100us } );

    fire_timer( t0 + 1 * 5ms + 100us );
    check_user_timers( t0, { 2 * 5ms + 100us } );

    // the first event is a little bit late
    at( t0_p + interval );
    synchronized_connection_event_callback_new_anchor( t0_p + interval, interval );

    // the next timer was not rescheduled, so it is still based on the same anchor
    // it was scheduled on, but the next timer is then based on t0_p
    fire_timer( t0 + 2 * 5ms + 100us );
    check_user_timers( t0_p, { 3 * 5ms + 100us } );

    fire_timer( t0_p + 3 * 5ms + 100us );
    check_user_timers( t0_p, { 4 * 5ms + 100us } );

    // the second event is a little bit early
    at( t0_m + 2 * interval );
    synchronized_connection_event_callback_new_anchor( t0_m + 2 * interval, interval );

    fire_timer( t0_p + 4 * 5ms + 100us );
    check_user_timers( t0_m, { 5 * 5ms + 100us } );
}

// the current implementation sets up a timer as usually but calls the user callbacks
// only when required by latency. That's a suboptimal implementation, so a test
// should not rely on the observable behaviour, but instead rely on the callback pattern
BOOST_FIXTURE_TEST_CASE( using_latency, fixture_7ms_minus_100us )
{
    callbacks.planned_latency = { 0, 4u, 1u, 0, 17u };

    // effective period is 6ms (5*6ms == 30ms; 6ms < 7ms)
    const auto interval = 30ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );

    fire_timers_until( t0 + 1s );

    // without latency, the reported instant would be 0, 1, 2, 3, 4, 0, 1, and so on.
    // with latency, the number of invocations is skipped
    check_instants({
        0u, 1u,
            1u,     3u, 4u,
    //  0u, 1u, 2u, 3u, 4u,
    //  0u, 1u, 2u, 3u, 4u,
    //  0u, 1u, 2u, 3u, 4u,
                2u
    });
}

BOOST_FIXTURE_TEST_CASE( force_callback_call_while_using_latency, fixture_7ms_minus_100us )
{
    callbacks.planned_latency = { 2u, 2u, 2u, 2u, 2u, 2u };

    // effective period is 7ms (6*7ms == 42ms; 7ms <= 7ms)
    const auto interval = 42ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );

    // wait until the callback after the beginning of the second connection interval was
    // called
    fire_timers_until( t0 + interval + 10ms );
    force_synchronized_connection_event_callback();
    fire_timers_until( t0 + 10 * interval );

    check_instants( {
        0u,         3u,
        0u, 1u,         4u,
            1u,         4u,   } );
}

BOOST_FIXTURE_TEST_CASE( correct_instant, fixture_7ms_minus_100us )
{
    // effective period is 6ms (5*6ms == 30ms; 6ms < 7ms)
    const auto interval = 30ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );

    fire_timers_until( t0 + 1s );

    check_instants( {
        0u, 1u, 2u, 3u, 4u,
        0u, 1u, 2u, 3u, 4u,
        0u } );
}

BOOST_FIXTURE_TEST_CASE( use_default_constructed_connection_and_persist_connection, fixture_7ms_minus_100us )
{
    // effective period is 6ms (5*6ms == 30ms; 6ms < 7ms)
    const auto interval = 30ms;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );

    fire_timers_until( t0 + 1s );

    check_connection_values( { 0, 1, 2, 3, 4, 5, 6 } );
}

BOOST_FIXTURE_TEST_CASE( reset_connection_after_reconnect, server_15ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_control_pdu( remote_user_terminated );

    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    run();

    check_connection_values( { 0, 1, 0, 1, 2 } );
}

/*
 * Test with MinimumPeriodUS equal a multiple of the interval
 */
using server_60ms_minus_100us  = unconnected_server< 60000, -100 >;
using fixture_60ms_plus_100us  = fixture< 60000,  100 >;

BOOST_FIXTURE_TEST_CASE( larger_min_period, fixture_60ms_plus_100us )
{
    // effective period 60ms: before every second event, each timer scheduled one interval after the last anchor
    const auto interval = 30ms;
    const auto t0_1 = t0 + 4us;
    const auto t0_2 = t0 + 6us;
    const auto t0_3 = t0 - 6us;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );

    check_user_timers( t0, { 1 * 60ms + 100us } );

    at(t0_1 + 1 * interval);
    synchronized_connection_event_callback_new_anchor( t0_1 + 1 * interval, interval );

    at(t0_2 + 2 * interval);
    synchronized_connection_event_callback_new_anchor( t0_2 + 2 * interval, interval );

    fire_timer( t0 + 1 * 60ms + 100us );
    check_user_timers( t0_2, { 2 * 60ms + 100us } );

    at(t0_2 + 3 * interval);
    synchronized_connection_event_callback_new_anchor( t0_2 + 3 * interval, interval );

    at(t0_3 + 4 * interval);
    synchronized_connection_event_callback_new_anchor( t0_3 + 4 * interval, interval );

    fire_timer( t0_2 + 2 * 60ms + 100us );
    check_user_timers( t0_3, { 3 * 60ms + 100us } );

}

BOOST_FIXTURE_TEST_CASE( larger_min_period_instant, server_60ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 2 );

    run();

    check_instants( { 0u, 0u, 0u, 0u } );
}

using server_20ms_minus_100us  = unconnected_server< 20000, -100 >;
using fixture_20ms_minus_100us = fixture< 20000, -100 >;

BOOST_FIXTURE_TEST_CASE( reconnect_with_different_interval, fixture_20ms_minus_100us )
{
    // starting with an effective periode of 15ms
    const auto interval = 30ms;

    // next an effective periode of 20ms
    const auto interval2 = 20ms;

    const auto t1 = t0 + 1s;

    at(t0);
    synchronized_connection_event_callback_new_connection( t0, interval );

    fire_timers_until( t0 + 2 * interval );

    check_user_timers( t0, {
        1 * 15ms - 100us,
        2 * 15ms - 100us,
        3 * 15ms - 100us,
        4 * 15ms - 100us,
        5 * 15ms - 100us } );

    synchronized_connection_event_callback_disconnect();

    at(t1);
    synchronized_connection_event_callback_new_connection( t1, interval2 );

    fire_timers_until( t1 + 2 * interval2 );


    check_user_timers( t1, {
        1 * 20ms - 100us,
        2 * 20ms - 100us,
        3 * 20ms - 100us } );

    check_instants( {
        0u, 1u, 0u, 1u,
        0u, 0u  } );
}

BOOST_FIXTURE_TEST_CASE( changed_interval_on_connection_update, server_20ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    add_connection_update_request( update_to_10ms_interval );
    ll_empty_pdus( 30 );
    run();

    // the timers of the 30ms interval, then, from the instant on, those of the 10ms interval
    check_user_timers( {
        15ms - 100us + ev(0),
        30ms - 100us + ev(0),
    } );

    check_user_timers( 13, {
        30ms - 100us + ev(6),
        20ms - 100us + ev(8),
        30ms - 100us + ev(9),
        30ms - 100us + ev(11)
    } );
}

struct callbacks_with_optional_callbacks_t
{
    struct connection {
        int value = 0;
    };

    unsigned ll_synchronized_callback( unsigned, connection& )
    {
        return 0;
    }

    connection ll_synchronized_callback_connect(
        bluetoe::link_layer::delta_time connection_interval,
        unsigned                        calls_per_interval )
    {
        out << "connect: " << connection_interval << ", " << calls_per_interval << "\n";

        return connection();
    }

    void ll_synchronized_callback_period_update(
        bluetoe::link_layer::delta_time connection_interval,
        unsigned                        calls_per_interval,
        connection&                     con )
    {
        ++con.value;
        out << "update: " << connection_interval << ", " << calls_per_interval << ", " << con.value << "\n";
    }

    void ll_synchronized_callback_disconnect( connection& con )
    {
        ++con.value;
        out << "disconnect: " << con.value << "\n";
    }

    std::ostringstream out;
} callbacks_with_optional_callbacks;

template < unsigned MinimumPeriodUS, int PhaseShiftUS, unsigned MaximumExecutionTimeUS = 0 >
struct unconnected_server_with_cbs : unconnected_base_t<
    test::small_temperature_service,
    test::radio_with_user_timer,
    bluetoe::link_layer::synchronized_connection_event_callback< callbacks_with_optional_callbacks_t, callbacks_with_optional_callbacks, MinimumPeriodUS, PhaseShiftUS, MaximumExecutionTimeUS >
 >
 {
    unconnected_server_with_cbs()
    {
        callbacks_with_optional_callbacks = callbacks_with_optional_callbacks_t();
    }

    std::vector< std::string > history() const
    {
        std::vector<std::string> result;

        std::stringstream input( callbacks_with_optional_callbacks.out.str() );
        std::string item;

        while ( std::getline( input, item, '\n' ) )
            result.push_back( item );

        return result;
    }
 };

using server_20ms_minus_100us_cbs  = unconnected_server_with_cbs< 20000, -100 >;

BOOST_FIXTURE_TEST_CASE( no_callbacks, server_20ms_minus_100us_cbs )
{
    BOOST_TEST( callbacks_with_optional_callbacks.out.str() == "" );
}

BOOST_FIXTURE_TEST_CASE( connect_disconnect_callbacks, server_20ms_minus_100us_cbs )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_control_pdu( remote_user_terminated );
    ll_empty_pdu();
    run();

    const auto hist = history();
    BOOST_REQUIRE_GT( hist.size(), 1u );

    BOOST_TEST( hist[ 0 ] == "connect: 30ms, 2" );
    BOOST_TEST( hist[ 1 ] == "disconnect: 1" );
}

BOOST_FIXTURE_TEST_CASE( update_callback, server_20ms_minus_100us_cbs )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    add_connection_update_request( update_to_10ms_interval );
    ll_empty_pdus( 10 );
    run();

    const auto hist = history();
    BOOST_REQUIRE_GT( hist.size(), 1u );

    BOOST_TEST( hist[ 0 ] == "connect: 30ms, 2" );
    BOOST_TEST( hist[ 1 ] == "update: 10ms, 0, 1" );
}

BOOST_FIXTURE_TEST_CASE( instants_of_a_call_spanning_intervals_with_positive_phase, fixture_60ms_plus_100us )
{
    const auto interval = 30ms;

    at( t0 );
    synchronized_connection_event_callback_new_connection( t0, interval );
    fire_timers_until( t0 + 10 * interval );

    check_instants( { 0u, 0u, 0u, 0u } );
}