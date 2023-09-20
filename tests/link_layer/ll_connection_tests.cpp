#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include "buffer_io.hpp"

struct only_one_pdu_from_central : unconnected
{
    only_one_pdu_from_central()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_event_respond( { 1, 0 } );

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

void add_channel_map_request( unconnected& c, std::uint16_t instance, std::uint64_t map )
{
    c.ll_control_pdu( {
        0x01,                                                   // LL_CHANNEL_MAP_IND
        static_cast< std::uint8_t >( map >> 0 ),                // map
        static_cast< std::uint8_t >( map >> 8 ),
        static_cast< std::uint8_t >( map >> 16 ),
        static_cast< std::uint8_t >( map >> 24 ),
        static_cast< std::uint8_t >( map >> 32 ),
        static_cast< std::uint8_t >( instance >> 0 ),           // instance
        static_cast< std::uint8_t >( instance >> 8 )
    } );
}

void add_empty_pdus( unconnected& c, unsigned count )
{
    c.add_empty_pdus( count );
}

void add_ll_timeouts( unconnected& c, unsigned count )
{
    c.add_ll_timeouts( count );
}

template < std::uint16_t Instance = 6, std::uint64_t Map = 0x1555555555, unsigned EmptyPDUs = Instance + 2 >
struct setup_connect_and_channel_map_request_base : unconnected
{
    static_assert( Instance > 0, "Instance > 0" );

    setup_connect_and_channel_map_request_base()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_channel_map_request( *this, Instance, Map );
        add_empty_pdus( EmptyPDUs );
    }

};

template < std::uint16_t Instance = 6, std::uint64_t Map = 0x1555555555, unsigned EmptyPDUs = Instance + 2 >
struct connect_and_channel_map_request_base : setup_connect_and_channel_map_request_base< Instance, Map, EmptyPDUs >
{
    connect_and_channel_map_request_base()
    {
        this->run();
    }
};

using instance_in_past = connect_and_channel_map_request_base< 0xffff, 0x1555555555, 20 >;

/*
 * When the instance is in the past, the peripheral should consider the connection to be lost.
 * The bluetoe behaviour is to go back and advertise.
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_with_instance_in_past, instance_in_past )
{
    BOOST_CHECK_EQUAL( connection_events().size(), 1u );
}

BOOST_FIXTURE_TEST_CASE( channel_map_request_with_wrong_size, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );

    ll_control_pdu( {
        0x01,                                                   // LL_CHANNEL_MAP_IND
        0xff, 0xff, 0xff, 0xff, 0x00,                           // map
        0, 8,                                                   // instance
        0xaa                                                    // ups, too large
    } );

    ll_empty_pdu();                                             // the response is expected to this connection event

    run();

    static constexpr std::uint8_t expected_response[] = {
        0x03, 0x02,             // ll header
        0x07, 0x01
    };

    auto response = connection_events().at( 1u ).transmitted_data.front().data;
    response.at( 0 ) &= 0x03;

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::end( response ), std::begin( expected_response ), std::end( expected_response ) );
}

using connect_and_channel_map_request = connect_and_channel_map_request_base<>;

/*
 * In this test, the odd channels are removed in connection event number 6. 19 channels remain.
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request, connect_and_channel_map_request )
{
    static constexpr unsigned expected_hop_sequence[] = {
        10, 20, 30, 3, 13, 23, // connection event 0-5
        28, 6
    };

    for ( unsigned i = 0; i != sizeof( expected_hop_sequence ) / sizeof( expected_hop_sequence[ 0 ] ); ++i )
    {
        BOOST_CHECK_EQUAL( connection_events().at( i ).channel, expected_hop_sequence[ i ] );
    }
}

using setup_connect_and_channel_map_request = setup_connect_and_channel_map_request_base<>;

/*
 * Make sure, that after a channel map request, the link layer still works as expected
 */
BOOST_FIXTURE_TEST_CASE( link_layer_still_active_after_channel_map_request, setup_connect_and_channel_map_request )
{
    ll_control_pdu( { 0x12 } ); // this will be send within the 10th connection event
    ll_empty_pdu();             // the response is expected to this connection event
    run();

    // and the response is send with the 11th event
    BOOST_REQUIRE( !connection_events().at( 10 ).transmitted_data.empty() );
    auto pdu = connection_events().at( 10 ).transmitted_data.front().data;
    BOOST_REQUIRE( !pdu.empty() );
    pdu[ 0 ] &= 0x3;

    static constexpr std::uint8_t expected_response[] = { 0x03, 0x01, 0x13 };
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( pdu ), std::end( pdu ), std::begin( expected_response ), std::end( expected_response ) );
}

using channel_map_request_with_one_timeout_fixture = setup_connect_and_channel_map_request_base< 5, 0x1000000001, 2 >;

/*
 * This test should make sure that connEventCounter is incremented, event when an connection event timed out
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_with_one_timeout, channel_map_request_with_one_timeout_fixture )
{
    add_connection_event_respond_timeout(); // event 3
    ll_empty_pdu();
    ll_empty_pdu();                         // event 5; here the map change is applied
    ll_empty_pdu();

    run();

    static constexpr unsigned expected_hop_sequence[] = {
        10, 20, 30, 3, 13, // connection event 0-4
        36, 36
    };

    for ( unsigned i = 0; i != sizeof( expected_hop_sequence ) / sizeof( expected_hop_sequence[ 0 ] ); ++i )
    {
        BOOST_CHECK_EQUAL( connection_events().at( i ).channel, expected_hop_sequence[ i ] );
    }
}

/*
 * This test should make sure that even when the map change is to be applied on an event, where a timeout
 * occures, that the map change is applied correctly
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_within_timeout, channel_map_request_with_one_timeout_fixture )
{
    ll_empty_pdu(); // event 3
    add_connection_event_respond_timeout();
    ll_empty_pdu(); // event 5; here the map change is applied
    ll_empty_pdu();

    run();

    static constexpr unsigned expected_hop_sequence[] = {
        10, 20, 30, 3, 13, // connection event 0-4
        36, 36
    };

    for ( unsigned i = 0; i != sizeof( expected_hop_sequence ) / sizeof( expected_hop_sequence[ 0 ] ); ++i )
    {
        BOOST_CHECK_EQUAL( connection_events().at( i ).channel, expected_hop_sequence[ i ] );
    }
}

struct channel_map_request_after_connection_count_wrap_fixture : unconnected
{
    channel_map_request_after_connection_count_wrap_fixture()
    {
        respond_to( 37, valid_connection_request_pdu );

        add_empty_pdus( 0x10000 - 4 );
        add_channel_map_request( *this, 2, 0x1000000001 );
        add_empty_pdus( 8 );
    }
};

#if 0
#ifndef BLUETOE_EXCLUDE_SLOW_TESTS

/*
 * This test should make sure the instance is correctly interpreted after the connect count wrapped from 0xffff to 0x0000
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_after_connection_count_wrap, channel_map_request_after_connection_count_wrap_fixture )
{
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

/*
 * connection update procedure with a "instance" in the past results in falling back to advertising
 */
BOOST_FIXTURE_TEST_CASE( connection_update_in_the_past, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_update_request( 5, 5, 40, 1, 200, 0x8002 );
    add_empty_pdus( 5 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), 1u );
}

struct connected_and_valid_connection_update_request_with_peripheral_latency : unconnected
{
    connected_and_valid_connection_update_request_with_peripheral_latency()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_update_request( 5, 6, 40, 1, 200, 6 );
        add_empty_pdus( 20 );

        run();
    }
};

struct connected_and_valid_connection_update_request : unconnected
{
    connected_and_valid_connection_update_request()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_update_request( 5, 6, 40, 0, 200, 6 );
        add_empty_pdus( 20 );

        run();
    }
};

/*
 * The cummulated sleep clock accuracies of central and peripheral is 550ppm (50ppm + 500ppm)
 * The old connection interval is 30ms
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_transmit_window, connected_and_valid_connection_update_request_with_peripheral_latency )
{
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
BOOST_FIXTURE_TEST_CASE( connection_update_correct_interval_used_with_latency, connected_and_valid_connection_update_request_with_peripheral_latency )
{
    auto const evt = connection_events()[ 7 ];

    const bluetoe::link_layer::delta_time event_start( 2 * 50000 );

    BOOST_CHECK_EQUAL( evt.start_receive, event_start - event_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( evt.end_receive, event_start + event_start.ppm( 550 ) );
}

/*
 * The cummulated sleep clock accuracies of central and peripheral is 550ppm (50ppm + 500ppm)
 * The new connection interval is 50ms, the new peripheral latency is still 0
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_interval_used, connected_and_valid_connection_update_request )
{
    auto const evt = connection_events()[ 7 ];

    const bluetoe::link_layer::delta_time event_start( 50000 );

    BOOST_CHECK_EQUAL( evt.start_receive, event_start - event_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( evt.end_receive, event_start + event_start.ppm( 550 ) );
}

/*
 * The old connection timeout is 720ms, the new connection timeout is 250ms
 * The new connection interval is 50ms, so after 5 connection events with timeout, the connection is timed out.
 */
BOOST_FIXTURE_TEST_CASE( connection_update_correct_timeout_used, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_update_request( 5, 6, 40, 0, 25, 6 );
    add_empty_pdus( 6 );
    add_ll_timeouts( 10 );

    run();

    BOOST_CHECK_EQUAL( connection_events().size(), std::size_t{ 8 + 4 } );
}

static void simulate_connection_update_request(
    unconnected& c, std::uint8_t win_size, std::uint16_t win_offset, std::uint16_t interval,
    std::uint16_t latency, std::uint16_t timeout, std::uint16_t instance )
{
    c.respond_to( 37, valid_connection_request_pdu );
    c.add_connection_update_request( win_size, win_offset, interval, latency, timeout, instance );
    c.add_empty_pdus( 10 );

    c.run();
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_window_size, unconnected )
{
    simulate_connection_update_request( *this, 205, 6, 40, 1, 25, 6 );

    BOOST_CHECK_EQUAL( connection_events().size(), 6u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_window_size_0, unconnected )
{
    simulate_connection_update_request( *this, 5, 0, 40, 0, 25, 6 );

    BOOST_CHECK_EQUAL( connection_events().size(), 16u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_window_offset, unconnected )
{
    simulate_connection_update_request( *this, 5, 206, 40, 1, 25, 6 );

    BOOST_CHECK_EQUAL( connection_events().size(), 6u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_interval, unconnected )
{
    simulate_connection_update_request( *this, 5, 6, 3200, 1, 25, 6 );

    BOOST_CHECK_EQUAL( connection_events().size(), 6u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_latency, unconnected )
{
    simulate_connection_update_request( *this, 5, 6, 40, 500, 25, 6 );

    BOOST_CHECK_EQUAL( connection_events().size(), 6u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_timeout, unconnected )
{
    simulate_connection_update_request( *this, 5, 6, 40, 1, 3300, 6 );

    BOOST_CHECK_EQUAL( connection_events().size(), 6u );
}

BOOST_FIXTURE_TEST_CASE( connection_update_request_invalid_instance, unconnected )
{
    simulate_connection_update_request( *this, 5, 6, 40, 1, 25, 1 );

    BOOST_CHECK_EQUAL( connection_events().size(), 1u );
}

/*
 * Test starts by having a LL_CONNECTION_UPDATE_REQ and at the instants connection event,
 * the server does not send a PDU. The peripheral still have to maintain the connection and
 * have to listen for the central the next but one connection interval.
 *
 * The centrals clock accuracy is 50ppm and the peripheral is configured with
 * the default of 500ppm (in sum 550ppm).
 */
BOOST_FIXTURE_TEST_CASE( connection_update_missing_central_pdu_at_instant, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_update_request( 6, 3, 6, 66, 198, 6 );
    add_empty_pdus( 5 );
    add_connection_event_respond_timeout();
    add_empty_pdus( 5 );

    run();

    const auto at_instant  = connection_events().at( 6 );
    const auto next        = connection_events().at( 7 );

    // Timeout at_instance
    BOOST_REQUIRE( at_instant.transmitted_data.empty() && at_instant.received_data.empty() );

    // transmitWindowSize   = 7.5ms
    // transmitWindowOffset = 3.75ms
    // connIntervalOLD      = 30ms
    // connIntervalnew      = 7.5ms

    // connIntervalOLD + transmitWindowOffset + connIntervalnew
    bluetoe::link_layer::delta_time window_start( 3750 + 7500 + 30000 );
    // connIntervalOLD + transmitWindowOffset + connIntervalnew + transmitWindowSize
    bluetoe::link_layer::delta_time window_end = window_start + bluetoe::link_layer::delta_time( 7500 );

    window_start -= window_start.ppm( 550 );
    window_end   += window_end.ppm( 550 );

    BOOST_REQUIRE_LT( window_start, window_end );

    BOOST_CHECK_EQUAL( next.start_receive, window_start );
    BOOST_CHECK_EQUAL( next.end_receive, window_end );
}

BOOST_FIXTURE_TEST_CASE( connection_update_missing_central_pdu_before_and_at_instant, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_update_request( 6, 3, 6, 66, 198, 6 );
    add_empty_pdus( 4 );
    add_connection_event_respond_timeout();
    add_connection_event_respond_timeout();
    add_empty_pdus( 5 );

    run();

    const auto before      = connection_events().at( 5 );
    const auto at_instant  = connection_events().at( 6 );
    const auto behind      = connection_events().at( 7 );

    // transmitWindowSize   = 7.5ms
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
    window_end   = window_start + bluetoe::link_layer::delta_time( 7500 );

    BOOST_CHECK_EQUAL( at_instant.start_receive, window_start - window_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( at_instant.end_receive, window_end + window_end.ppm( 550 ) );

    // the event after the missing event at instant is moved further by connIntervalnew
    window_start += bluetoe::link_layer::delta_time( 7500 );
    window_end   = window_start + bluetoe::link_layer::delta_time( 7500 );

    BOOST_CHECK_EQUAL( behind.start_receive, window_start - window_start.ppm( 550 ) );
    BOOST_CHECK_EQUAL( behind.end_receive, window_end + window_end.ppm( 550 ) );
}

BOOST_FIXTURE_TEST_CASE( response_to_an_feature_request, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_control_pdu({
        0x08,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    });
    ll_empty_pdu();

    run();

    auto response = connection_events().at( 1 ).transmitted_data.at( 0 );
    response[ 0 ] &= 0x03;

    static const std::uint8_t expected_response[] = {
        0x03, 0x09,
        0x09,
        0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::end( response ), std::begin( expected_response ), std::end( expected_response ) );
}
