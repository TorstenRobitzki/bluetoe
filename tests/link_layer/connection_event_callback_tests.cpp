#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/connection_event_callback.hpp>

#include "connected.hpp"

struct callbacks_t {
    callbacks_t()
    {
    }

    struct connection {
        connection() : value( 0 ) {}

        int value;
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
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    const auto timers = scheduled_user_timers();

    BOOST_REQUIRE_GT( timers.size(), 2u );

    BOOST_CHECK_EQUAL( timers[ 0 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 1 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
}

BOOST_FIXTURE_TEST_CASE( callback_called_with_correct_period_and_positive_phase, server_7ms_plus_100us )
{
    // 30ms interval -> effective period: 6ms
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    const auto timers = scheduled_user_timers();

    BOOST_REQUIRE_GT( timers.size(), 2u );

    BOOST_CHECK_EQUAL( timers[ 0 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) + bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 1 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
}

BOOST_FIXTURE_TEST_CASE( using_latency, server_7ms_minus_100us )
{
    callbacks.planned_latency = { 0, 4u, 1u, 0, 17u };
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    const auto timers = scheduled_user_timers();

    BOOST_REQUIRE_GT( timers.size(), 5u );

    BOOST_CHECK_EQUAL( timers[ 0 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 1 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
    BOOST_CHECK_EQUAL( timers[ 2 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) * 5u );
    BOOST_CHECK_EQUAL( timers[ 3 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) * 2u );
    BOOST_CHECK_EQUAL( timers[ 4 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
    BOOST_CHECK_EQUAL( timers[ 5 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) * 18u );
}

BOOST_FIXTURE_TEST_CASE( correct_instance, server_7ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    static const auto expected = {
        0u, 1u, 2u, 3u, 4u,
        0u, 1u, 2u, 3u, 4u,
        0u };

    BOOST_TEST(
        take( callbacks.instants, expected.size() ) == expected,
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( correct_instance_with_instance, server_7ms_minus_100us )
{
    callbacks.planned_latency = { 0, 4u, 1u, 0, 2u };
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    static const auto expected = {
        0u, 1u,
            1u,    3u, 4u,
                2u };

    BOOST_TEST(
        take( callbacks.instants, expected.size() ) == expected,
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( use_default_constructed_connection_and_persist_connection, server_7ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    const auto connection_values = callbacks.connection_values;

    BOOST_REQUIRE_GT( connection_values.size(), 5u );

    BOOST_CHECK_EQUAL( connection_values[ 0 ], 0 );
    BOOST_CHECK_EQUAL( connection_values[ 1 ], 1 );
    BOOST_CHECK_EQUAL( connection_values[ 2 ], 2 );
}

BOOST_FIXTURE_TEST_CASE( reset_connection_after_reconnect, server_15ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_control_pdu( {
        0x02,           // LL_TERMINATE_IND
        0x13            // REMOTE USER TERMINATED CONNECTION
    } );

    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    run();

    static const auto expected = { 0, 1, 0, 1, 2 };

    BOOST_TEST(
        take( callbacks.connection_values, expected.size() ) == expected,
        boost::test_tools::per_element() );
}

/*
 * Test with MinimumPeriodUS equal a fraction of the interval
 */

/*
 * Test with MinimumPeriodUS equal a fraction of the interval - 1
 */

/*
 * Compile time check, that there can be no rounding errors
 */