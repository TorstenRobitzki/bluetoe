#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/connection_event_callback.hpp>

#include "connected.hpp"

struct callbacks_t {
    callbacks_t()
        : last_instant( ~0 )
        , call_back_called( 0 )
    {
    }

    struct connection {};

    unsigned ll_synchronized_callback( unsigned instant, connection& )
    {
        last_instant = instant;

        return 0;
    }

    unsigned last_instant;
    unsigned call_back_called;

} callbacks;

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
using unconnected_server = unconnected_base_t<
    test::small_temperature_service,
    test::radio_with_user_timer,
    bluetoe::link_layer::synchronized_connection_event_callback< callbacks_t, callbacks, MinimumPeriodUS, PhaseShiftUS, MaximumExecutionTimeUS >
 >;

using server_7ms_minus_100us = unconnected_server< 7000, -100 >;
using server_7ms_plus_100us = unconnected_server< 7000, 100 >;

BOOST_FIXTURE_TEST_CASE( callback_not_called_if_unconnected, server_7ms_minus_100us )
{
    run();

    BOOST_CHECK_EQUAL( callbacks.call_back_called, 0u );
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

/*
 * Test with MinimumPeriodUS equal a fraction of the interval
 */

/*
 * Test with MinimumPeriodUS equal a fraction of the interval - 1
 */

/*
 * Compile time check, that there can be no rounding errors
 */