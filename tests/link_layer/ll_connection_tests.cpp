#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include "buffer_io.hpp"

using namespace test;

struct only_one_pdu_from_central : unconnected
{
    only_one_pdu_from_central()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_event_respond( ll_empty() );

        run();
    }

};

/*
 * The connection interval is 30ms, the centrals clock accuracy is 50ppm and the peripheral is configured with
 * the default of 500ppm (in sum 550ppm).
 * So the maximum derivation is 16µs
 */
BOOST_FIXTURE_TEST_CASE( smaller_window_after_connected, only_one_pdu_from_central )
{
    BOOST_REQUIRE_GE( connection_events().size(), 1u );
    auto event = connection_events()[ 1 ];

    BOOST_CHECK_EQUAL( event.start_receive, bluetoe::link_layer::delta_time::usec( 30000 - 16 ) );
    BOOST_CHECK_EQUAL( event.end_receive, bluetoe::link_layer::delta_time::usec( 30000 + 16 ) );
}

/*
 * For the second connection event, the derivation from the 2*30ms is 33µs (+ 1µs extra for rounding)
 */
BOOST_FIXTURE_TEST_CASE( window_size_is_increasing_with_connection_event_timeouts, only_one_pdu_from_central )
{
    BOOST_REQUIRE_GE( connection_events().size(), 2u );
    auto event = connection_events()[ 2 ];

    BOOST_CHECK( event.start_receive >= bluetoe::link_layer::delta_time::usec( 60000 - 34 ) );
    BOOST_CHECK( event.start_receive <= bluetoe::link_layer::delta_time::usec( 60000 - 32 ) );
    BOOST_CHECK( event.end_receive >= bluetoe::link_layer::delta_time::usec( 60000 + 32 ) );
    BOOST_CHECK( event.end_receive <= bluetoe::link_layer::delta_time::usec( 60000 + 34 ) );

}

/*
 * Once the link layer received a PDU from the central, the supervision timeout is in charge
 * In this example, the timeout is 720ms, the connection interval is 30ms, so the timeout is
 * reached after 24 connection intervals (that's the 25th schedule request).
 */
BOOST_FIXTURE_TEST_CASE( supervision_timeout_is_in_charge, only_one_pdu_from_central )
{
    BOOST_CHECK_EQUAL( connection_events().size(), 25u );
}

// the channel maps the tests switch to: the even channels, and channel 0 and 36
static constexpr std::uint64_t even_channels      = 0x1555555555;
static constexpr std::uint64_t channels_0_and_36  = 0x1000000001;

/*
 * The channel map tests: connected, the central sends an LL_CHANNEL_MAP_IND for `instant` and
 * then empty PDUs, so that the events up to and past the instant happen.
 */
struct channel_map : unconnected
{
    void connect_and_send_channel_map( std::uint16_t instant, std::uint64_t map, unsigned empty_pdus )
    {
        respond_to( 37, valid_connection_request_pdu );
        ll_control_pdu( ll_channel_map_ind( map, instant ) );
        add_empty_pdus( empty_pdus );
    }
};

/*
 * When the instance is in the past, the peripheral should consider the connection to be lost.
 * The bluetoe behaviour is to go back and advertise.
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_with_instance_in_past, channel_map )
{
    connect_and_send_channel_map( 0xffff, even_channels, 20 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), 1u );
}

BOOST_FIXTURE_TEST_CASE( channel_map_request_with_wrong_size, unconnected )
{
    // an LL_CHANNEL_MAP_IND with a byte too many
    check_single_ll_control_pdu(
        ll_control( { 0x01, 0xff, 0xff, 0xff, 0xff, 0x00, 0, 8, 0xaa } ),
        ll_control( ll_unknown_rsp( 0x01 ) ) );
}

/*
 * In this test, the odd channels are removed in connection event number 6. 19 channels remain.
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request, channel_map )
{
    connect_and_send_channel_map( 6, even_channels, 8 );

    run();

    check_channels( {
        10, 20, 30, 3, 13, 23, // connection event 0-5
        28, 6 } );
}

/*
 * Make sure, that after a channel map request, the link layer still works as expected
 */
BOOST_FIXTURE_TEST_CASE( link_layer_still_active_after_channel_map_request, channel_map )
{
    connect_and_send_channel_map( 6, even_channels, 8 );

    ll_control_pdu( ll_ping_req() );    // this will be send within the 10th connection event
    ll_empty_pdu();                     // the response is expected to this connection event

    run();

    // and the response is send with the 11th event
    check_transmitted( 10, ll_control( ll_ping_rsp() ) );
}

/*
 * This test should make sure that connEventCounter is incremented, event when an connection event timed out
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_with_one_timeout, channel_map )
{
    connect_and_send_channel_map( 5, channels_0_and_36, 2 );

    add_connection_event_respond_timeout(); // event 3
    ll_empty_pdu();
    ll_empty_pdu();                         // event 5; here the map change is applied
    ll_empty_pdu();

    run();

    check_channels( {
        10, 20, 30, 3, 13, // connection event 0-4
        36, 36 } );
}

/*
 * This test should make sure that even when the map change is to be applied on an event, where a timeout
 * occures, that the map change is applied correctly
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_within_timeout, channel_map )
{
    connect_and_send_channel_map( 5, channels_0_and_36, 2 );

    ll_empty_pdu(); // event 3
    add_connection_event_respond_timeout();
    ll_empty_pdu(); // event 5; here the map change is applied
    ll_empty_pdu();

    run();

    check_channels( {
        10, 20, 30, 3, 13, // connection event 0-4
        36, 36 } );
}

#if 0
#ifndef BLUETOE_EXCLUDE_SLOW_TESTS

/*
 * This test should make sure the instance is correctly interpreted after the connect count wrapped from 0xffff to 0x0000
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_after_connection_count_wrap, channel_map )
{
    respond_to( 37, valid_connection_request_pdu );
    add_empty_pdus( 0x10000 - 4 );
    ll_control_pdu( ll_channel_map_ind( channels_0_and_36, 2 ) );
    add_empty_pdus( 8 );

    end_of_simulation( bluetoe::link_layer::delta_time::seconds( 3000 ) );
    run();

    BOOST_CHECK_EQUAL( connection_events().at( 0x10002 ).channel, 36u );
    BOOST_CHECK_EQUAL( connection_events().at( 0x10003 ).channel, 36u );
    BOOST_CHECK_EQUAL( connection_events().at( 0x10004 ).channel, 36u );
}

#endif
#endif

BOOST_FIXTURE_TEST_CASE( l2cap_data_during_channel_map_request_with_buffer_big_enough_for_two_pdus, only_one_pdu_from_central )
{
    /// @TODO implement
}

BOOST_FIXTURE_TEST_CASE( l2cap_data_during_channel_map_request_with_buffer_big_enough_for_only_one_pdus, only_one_pdu_from_central )
{
    /// @TODO implement
}

// a connection update the link layer accepts: to a 50 ms interval with a 200 ms timeout at instant 6
static const connection_update valid_update = {
    .window_size = 5, .window_offset = 6, .interval = 40, .latency = 0, .timeout = 200, .instant = 6 };

/*
 * The connection update tests: connected, the central sends an LL_CONNECTION_UPDATE_IND and then
 * empty PDUs, so that the events up to and past the instant happen.
 */
struct connection_update_procedure : unconnected
{
    void connect_and_update( const connection_update& update, unsigned empty_pdus = 20 )
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_update_request( update );
        add_empty_pdus( empty_pdus );
    }

    // an update the link layer must refuse leaves the connection to its supervision timeout
    void check_refused( const connection_update& update )
    {
        connect_and_update( update, 10 );

        run();

        BOOST_CHECK_EQUAL( connection_events().size(), 6u );
    }
};

/*
 * connection update procedure with a "instance" in the past results in falling back to advertising
 */
BOOST_FIXTURE_TEST_CASE( connection_update_in_the_past, connection_update_procedure )
{
    connect_and_update( with( valid_update, &connection_update::instant, 0x8002 ), 5 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), 1u );
}

/*
 * The cummulated sleep clock accuracies of central and peripheral is 550ppm (50ppm + 500ppm)
 * The old connection interval is 30ms
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_transmit_window, connection_update_procedure )
{
    connect_and_update( with( valid_update, &connection_update::latency, 1 ) );

    run();

    // The first event happend is 0 on which the connection update is send, the second is 1, due to
    // the outstanding acknowledgment, the third will then be at instance 3
    auto const evt = connection_events()[ 6 ];

    bluetoe::link_layer::delta_time window_start( 30000 + 7500 );
    bluetoe::link_layer::delta_time window_end( 30000 + 7500 + 6250 );
    window_start -= window_start.ppm( 550 );
    window_end   += window_end.ppm( 550 );

    BOOST_REQUIRE_LT( window_start, window_end );

    BOOST_CHECK_EQUAL( evt.start_receive, window_start );
    BOOST_CHECK_EQUAL( evt.end_receive, window_end );
    BOOST_CHECK_EQUAL( evt.channel, 70u % 37u );
}

/*
 * The cummulated sleep clock accuracies of central and peripheral is 550ppm (50ppm + 500ppm)
 * The new connection interval is 50ms, the new peripheral latency is 1
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_interval_used_with_latency, connection_update_procedure )
{
    connect_and_update( with( valid_update, &connection_update::latency, 1 ) );

    run();

    auto const evt = connection_events()[ 7 ];

    const bluetoe::link_layer::delta_time event_start( 2 * 50000 );

    BOOST_CHECK_EQUAL( evt.start_receive, event_start - event_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( evt.end_receive, event_start + event_start.ppm( 550 ) );
}

/*
 * The cummulated sleep clock accuracies of central and peripheral is 550ppm (50ppm + 500ppm)
 * The new connection interval is 50ms, the new peripheral latency is still 0
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_interval_used, connection_update_procedure )
{
    // the update valid_update names, by hand
    respond_to( 37, valid_connection_request_pdu );
    ll_control_pdu( {
        0x00,                           // LL_CONNECTION_UPDATE_IND
        0x05,                           // WinSize: 6.25 ms
        0x06, 0x00,                     // WinOffset: 7.5 ms
        0x28, 0x00,                     // Interval: 50 ms
        0x00, 0x00,                     // Latency
        0xc8, 0x00,                     // Timeout: 2 s
        0x06, 0x00                      // Instant
    } );
    add_empty_pdus( 20 );

    run();

    auto const evt = connection_events()[ 7 ];

    const bluetoe::link_layer::delta_time event_start( 50000 );

    BOOST_CHECK_EQUAL( evt.start_receive, event_start - event_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( evt.end_receive, event_start + event_start.ppm( 550 ) );
}

/*
 * The old connection timeout is 720ms, the new connection timeout is 250ms
 * The new connection interval is 50ms, so after 5 connection events with timeout, the connection is timed out.
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_timeout_used, connection_update_procedure )
{
    connect_and_update( with( valid_update, &connection_update::timeout, 25 ), 6 );
    add_ll_timeouts( 10 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), std::size_t{ 8 + 4 } );
}

// what the refused updates start from: a latency of 1 and a timeout of 250 ms
static const connection_update short_timeout_update = {
    .window_size = 5, .window_offset = 6, .interval = 40, .latency = 1, .timeout = 25, .instant = 6 };

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_window_size, connection_update_procedure )
{
    check_refused( with( short_timeout_update, &connection_update::window_size, 205 ) );
}

// a window of the interval's size leaves no room before the next interval; the update is refused
BOOST_FIXTURE_TEST_CASE( connection_update_request_window_size_equal_to_interval, connection_update_procedure )
{
    check_refused( with( with( short_timeout_update, &connection_update::window_size, 8 ), &connection_update::interval, 8 ) );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_window_size_0, connection_update_procedure )
{
    connect_and_update( with( with( short_timeout_update, &connection_update::window_offset, 0 ), &connection_update::latency, 0 ), 10 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), 16u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_window_offset, connection_update_procedure )
{
    check_refused( with( short_timeout_update, &connection_update::window_offset, 206 ) );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_interval, connection_update_procedure )
{
    check_refused( with( short_timeout_update, &connection_update::interval, 3200 ) );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_latency, connection_update_procedure )
{
    check_refused( with( short_timeout_update, &connection_update::latency, 500 ) );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_timeout, connection_update_procedure )
{
    check_refused( with( short_timeout_update, &connection_update::timeout, 3300 ) );
}

// an instant that already passed ends the connection at once
BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_instance, connection_update_procedure )
{
    connect_and_update( with( short_timeout_update, &connection_update::instant, 1 ), 10 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), 1u );
}

// an update to a 7.5 ms interval with a 3.75 ms window offset and a 5 ms window, latency 66
static const connection_update update_to_short_interval = {
    .window_size = 4, .window_offset = 3, .interval = 6, .latency = 66, .timeout = 198, .instant = 6 };

/*
 * Test starts by having a LL_CONNECTION_UPDATE_REQ and at the instants connection event,
 * the server does not send a PDU. The peripheral still have to maintain the connection and
 * have to listen for the central the next but one connection interval.
 *
 * The centrals clock accuracy is 50ppm and the peripheral is configured with
 * the default of 500ppm (in sum 550ppm).
 */
BOOST_FIXTURE_TEST_CASE( connection_update_missing_central_pdu_at_instant, connection_update_procedure )
{
    connect_and_update( update_to_short_interval, 5 );
    add_connection_event_respond_timeout();
    add_empty_pdus( 5 );

    run();

    const auto at_instant  = connection_events().at( 6 );
    const auto next        = connection_events().at( 7 );

    // Timeout at_instance
    BOOST_REQUIRE( at_instant.transmitted_data.empty() && at_instant.received_data.empty() );

    // transmitWindowSize   = 5ms
    // transmitWindowOffset = 3.75ms
    // connIntervalOLD      = 30ms
    // connIntervalnew      = 7.5ms

    // connIntervalOLD + transmitWindowOffset + connIntervalnew
    bluetoe::link_layer::delta_time window_start( 3750 + 7500 + 30000 );
    // connIntervalOLD + transmitWindowOffset + connIntervalnew + transmitWindowSize
    bluetoe::link_layer::delta_time window_end = window_start + bluetoe::link_layer::delta_time( 5000 );

    window_start -= window_start.ppm( 550 );
    window_end   += window_end.ppm( 550 );

    BOOST_REQUIRE_LT( window_start, window_end );

    BOOST_CHECK_EQUAL( next.start_receive, window_start );
    BOOST_CHECK_EQUAL( next.end_receive, window_end );
}

BOOST_FIXTURE_TEST_CASE( connection_update_missing_central_pdu_before_and_at_instant, connection_update_procedure )
{
    connect_and_update( update_to_short_interval, 4 );
    add_connection_event_respond_timeout();
    add_connection_event_respond_timeout();
    add_empty_pdus( 5 );

    run();

    const auto before      = connection_events().at( 5 );
    const auto at_instant  = connection_events().at( 6 );
    const auto behind      = connection_events().at( 7 );

    // transmitWindowSize   = 5ms
    // transmitWindowOffset = 3.75ms
    // connIntervalOLD      = 30ms
    // connIntervalnew      = 7.5ms

    // the first missing connection event was scheduled at connIntervalOLD based on prior connection events
    // ancor point.

    // connIntervalOLD + transmitWindowOffset + connIntervalnew
    bluetoe::link_layer::delta_time window_start( 30000 );
    // connIntervalOLD + transmitWindowOffset + connIntervalnew + transmitWindowSize
    bluetoe::link_layer::delta_time window_end = window_start;

    BOOST_CHECK_EQUAL( before.start_receive, window_start - window_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( before.end_receive, window_end + window_end.ppm( 550 ) );

    // and timed out:
    BOOST_REQUIRE( before.transmitted_data.empty() && before.received_data.empty() );

    // the next connection event is then at the instant
    window_start = bluetoe::link_layer::delta_time( 3750 + 30000 + 30000 );
    window_end   = window_start + bluetoe::link_layer::delta_time( 5000 );

    BOOST_CHECK_EQUAL( at_instant.start_receive, window_start - window_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( at_instant.end_receive, window_end + window_end.ppm( 550 ) );

    // the event after the missing event at instant is moved further by connIntervalnew
    window_start += bluetoe::link_layer::delta_time( 7500 );
    window_end   = window_start + bluetoe::link_layer::delta_time( 5000 );

    BOOST_CHECK_EQUAL( behind.start_receive, window_start - window_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( behind.end_receive, window_end + window_end.ppm( 550 ) );
}

BOOST_FIXTURE_TEST_CASE( response_to_an_feature_request, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_control_pdu( {
        0x08,                                           // LL_FEATURE_REQ
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: everything of the first octet
    } );
    ll_empty_pdu();

    run();

    check_transmitted( 1, {
        0x03, 0x09,                                     // LL control PDU
        0x09,                                           // LL_FEATURE_RSP
        0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: LE Encryption, Connection Parameters Request, LE Ping
    } );
}
