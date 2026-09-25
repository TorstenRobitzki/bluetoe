#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/connection_event_callback.hpp>

#include "connected.hpp"

#include <sstream>

using namespace test;
using bluetoe::link_layer::delta_time;

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

    /*
     * The delays of the scheduled user timers from the given one on, in milliseconds
     * before the phase shift is applied
     */
    void check_user_timers( std::size_t first, std::initializer_list< unsigned > delays_ms ) const
    {
        const auto timers = this->scheduled_user_timers();
        BOOST_REQUIRE_GE( timers.size(), first + delays_ms.size() );

        std::size_t index = first;

        for ( const auto ms : delays_ms )
        {
            const auto expected = PhaseShiftUS < 0
                ? delta_time::msec( ms ) - delta_time::usec( -PhaseShiftUS )
                : delta_time::msec( ms ) + delta_time::usec( PhaseShiftUS );

            BOOST_TEST_CONTEXT( "user timer " << index )
            {
                BOOST_CHECK_EQUAL( timers[ index ].delay, expected );
            }

            ++index;
        }
    }

    void check_user_timers( std::initializer_list< unsigned > delays_ms ) const
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

BOOST_FIXTURE_TEST_CASE( callback_not_called_if_unconnected, server_7ms_minus_100us )
{
    run();

    BOOST_CHECK( callbacks.connection_values.empty() );
    BOOST_CHECK_EQUAL( scheduled_user_timers().size(), 0u );
}

BOOST_FIXTURE_TEST_CASE( callback_called_with_correct_period_and_phase, server_7ms_minus_100us )
{
    // 30ms interval -> effective period: 6ms
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );

    run();

    check_user_timers( { 6, 12, 18, 24, 30, 36, 12 } );
}

BOOST_FIXTURE_TEST_CASE( callback_called_with_correct_period_and_positive_phase, server_7ms_plus_100us )
{
    // 30ms interval -> effective period: 6ms
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );

    run();

    check_user_timers( { 6, 12, 18, 24, 30, 6, 12 } );
}

BOOST_FIXTURE_TEST_CASE( using_latency, server_7ms_minus_100us )
{
    callbacks.planned_latency = { 0, 4u, 1u, 0, 17u };
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );

    run();

    check_user_timers( { 6, 12, 18, 24, 30, 36, 12 } );
}

BOOST_FIXTURE_TEST_CASE( force_callback_call_while_using_latency, server_7ms_minus_100us )
{
    callbacks.planned_latency = { 2u, 2u, 2u, 2u, 2u, 2u };
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_function_call([this](){
        force_synchronized_connection_event_callback();
    });
    ll_empty_pdu();

    run();

    check_instants( {
        0u,        3u,
        0u,        3u,    // at the second connection event, the
                          // callback is forced
            1u,        4u
    } );
}

BOOST_FIXTURE_TEST_CASE( correct_instant, server_7ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );

    run();

    check_instants( {
        0u, 1u, 2u, 3u, 4u,
        0u, 1u, 2u, 3u, 4u,
        0u } );
}

BOOST_FIXTURE_TEST_CASE( correct_instant_with_latency, server_7ms_minus_100us )
{
    callbacks.planned_latency = { 0, 4u, 1u, 0, 2u };
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );

    run();

    check_instants( {
        0u, 1u,
            1u,    3u, 4u,
                2u } );
}

BOOST_FIXTURE_TEST_CASE( use_default_constructed_connection_and_persist_connection, server_7ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );

    run();

    check_connection_values( { 0, 1, 2 } );
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

BOOST_FIXTURE_TEST_CASE( larger_min_period, server_60ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 30 );

    run();

    check_user_timers( { 60, 120, 120, 120, 120, 120, 120, 120 } );
}

BOOST_FIXTURE_TEST_CASE( larger_min_period_instant, server_60ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 2 );

    run();

    check_instants( { 0u, 0u, 0u, 0u } );
}

using server_20ms_minus_100us  = unconnected_server< 20000, -100 >;

BOOST_FIXTURE_TEST_CASE( reconnect_with_different_interval, server_20ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_control_pdu( remote_user_terminated );

    // the second connection with a 10ms interval
    respond_to( 37, connect_ind( { .window_offset = 8, .interval = 8 } ) );
    ll_empty_pdu();
    run();

    check_user_timers( { 15, 30, 20, 40 } );
}

BOOST_FIXTURE_TEST_CASE( changed_interval_on_connection_update, server_20ms_minus_100us )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    add_connection_update_request( update_to_10ms_interval );
    ll_empty_pdus( 30 );
    run();

    // the timers of the 30ms interval, then, from the instant on, those of the 10ms interval
    check_user_timers( { 15, 30 } );
    check_user_timers( 13, { 30, 20, 40, 40 } );
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
