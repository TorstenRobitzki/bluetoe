#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/connection_event_callback.hpp>

#include "connected.hpp"

#include <sstream>

struct callbacks_t {

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

using bluetoe::link_layer::delta_time;

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
    BOOST_CHECK_EQUAL( timers[ 2 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
    BOOST_CHECK_EQUAL( timers[ 3 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
    BOOST_CHECK_EQUAL( timers[ 4 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
    BOOST_CHECK_EQUAL( timers[ 5 ].delay, bluetoe::link_layer::delta_time::msec( 6 ) );
}

BOOST_FIXTURE_TEST_CASE( force_callback_call_while_using_latency, server_7ms_minus_100us )
{
    callbacks.planned_latency = { 2u, 2u, 2u, 2u, 2u, 2u };
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_function_call([this](){
        force_synchronized_connection_event_callback();
    });
    ll_empty_pdu();

    run();

    static const auto expected = {
        0u,        3u,
        0u,        3u,    // at the second connection event, the
                          // callback is forces
            1u,        4u
    };

    BOOST_TEST(
        take( callbacks.instants, expected.size() ) == expected,
        boost::test_tools::per_element() );
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
 * Test with MinimumPeriodUS equal a multiple of the interval
 */
using server_60ms_minus_100us  = unconnected_server< 60000, -100 >;

BOOST_FIXTURE_TEST_CASE( larger_min_period, server_60ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    const auto timers = scheduled_user_timers();

    BOOST_REQUIRE_GT( timers.size(), 3u );

    BOOST_CHECK_EQUAL( timers[ 0 ].delay, delta_time::msec( 60 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 1 ].delay, delta_time::msec( 60 ) );
    BOOST_CHECK_EQUAL( timers[ 2 ].delay, delta_time::msec( 60 ) );
    BOOST_CHECK_EQUAL( timers[ 3 ].delay, delta_time::msec( 60 ) );
}

BOOST_FIXTURE_TEST_CASE( larger_min_period_instance, server_60ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    static const auto expected = { 0u, 0u, 0u, 0u };

    BOOST_TEST(
        take( callbacks.instants, expected.size() ) == expected,
        boost::test_tools::per_element() );
}

using server_20ms_minus_100us  = unconnected_server< 20000, -100 >;

BOOST_FIXTURE_TEST_CASE( reconnect_with_different_interval, server_20ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_control_pdu( {
        0x02,           // LL_TERMINATE_IND
        0x13            // REMOTE USER TERMINATED CONNECTION
    } );

    this->respond_to( 37, {
        0xc5, 0x22,                         // header
        0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
        0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
        0x08, 0x81, 0xf6,                   // CRC Init
        0x03,                               // transmit window size
        0x08, 0x00,                         // window offset
        0x08, 0x00,                         // interval (10ms)
        0x00, 0x00,                         // peripheral latency
        0x48, 0x00,                         // connection timeout (720ms)
        0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
        0xaa                                // hop increment and sleep clock accuracy (10 and 50ppm)
    } );
    ll_empty_pdu();
    run();

    const auto timers = scheduled_user_timers();
    BOOST_REQUIRE_GT( timers.size(), 3u );

    BOOST_CHECK_EQUAL( timers[ 0 ].delay, delta_time::msec( 15 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 1 ].delay, delta_time::msec( 15 ) );

    BOOST_CHECK_EQUAL( timers[ 2 ].delay, delta_time::msec( 20 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 3 ].delay, delta_time::msec( 20 ) );
}

BOOST_FIXTURE_TEST_CASE( changed_interval_on_connection_update, server_20ms_minus_100us )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    add_connection_update_request(
        0x08, 0x08, 0x08, 0, 100, 8 );
    ll_empty_pdus( 10 );
    run();

    const auto timers = scheduled_user_timers();
    BOOST_REQUIRE_GT( timers.size(), 16u );

    // instance 0
    BOOST_CHECK_EQUAL( timers[ 0 ].delay, delta_time::msec( 15 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 1 ].delay, delta_time::msec( 15 ) );

    // ...
    BOOST_CHECK_EQUAL( timers[ 13 ].delay, delta_time::msec( 20 ) - bluetoe::link_layer::delta_time::usec( 100 ) );
    BOOST_CHECK_EQUAL( timers[ 14 ].delay, delta_time::msec( 20 ) );
    BOOST_CHECK_EQUAL( timers[ 15 ].delay, delta_time::msec( 20 ) );
    BOOST_CHECK_EQUAL( timers[ 16 ].delay, delta_time::msec( 20 ) );
}

struct callbacks_with_optional_callbacks_t
{
    struct connection {
        connection() : value( 0 ) {}

        int value;
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
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    ll_control_pdu( {
        0x02,           // LL_TERMINATE_IND
        0x13            // REMOTE USER TERMINATED CONNECTION
    } );
    ll_empty_pdu();
    run();

    const auto hist = history();
    BOOST_REQUIRE_GT( hist.size(), 1u );

    BOOST_TEST( hist[ 0 ] == "connect: 30ms, 2" );
    BOOST_TEST( hist[ 1 ] == "disconnect: 1" );
}

BOOST_FIXTURE_TEST_CASE( update_callback, server_20ms_minus_100us_cbs )
{
    this->respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdu();
    add_connection_update_request(
        0x08, 0x08, 0x08, 0, 100, 8 );
    ll_empty_pdus( 10 );
    run();

    const auto hist = history();
    BOOST_REQUIRE_GT( hist.size(), 1u );

    BOOST_TEST( hist[ 0 ] == "connect: 30ms, 2" );
    BOOST_TEST( hist[ 1 ] == "update: 10ms, 0, 1" );
}
