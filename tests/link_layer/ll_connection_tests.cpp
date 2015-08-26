#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include "buffer_io.hpp"

struct only_one_pdu_from_master : unconnected
{
    only_one_pdu_from_master()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_event_respond( { 1, 0 } );

        run();
    }

};

/*
 * The connection interval is 30ms, the masters clock accuracy is 50ppm and the slave is configured with
 * the default of 500ppm (in sum 550ppm).
 * So the maximum derivation is 16µs
 */
BOOST_FIXTURE_TEST_CASE( smaller_window_after_connected, only_one_pdu_from_master )
{
    BOOST_REQUIRE_GE( connection_events().size(), 1 );
    auto event = connection_events()[ 1 ];

    BOOST_CHECK_EQUAL( event.start_receive, bluetoe::link_layer::delta_time::usec( 30000 - 16 ) );
    BOOST_CHECK_EQUAL( event.end_receive, bluetoe::link_layer::delta_time::usec( 30000 + 16 ) );
}

/*
 * For the second connection event, the derivation from the 2*30ms is 33µs
 */
BOOST_FIXTURE_TEST_CASE( window_size_is_increasing_with_connection_event_timeouts, only_one_pdu_from_master )
{
    BOOST_REQUIRE_GE( connection_events().size(), 2 );
    auto event = connection_events()[ 2 ];

    BOOST_CHECK_EQUAL( event.start_receive, bluetoe::link_layer::delta_time::usec( 60000 - 33 ) );
    BOOST_CHECK_EQUAL( event.end_receive, bluetoe::link_layer::delta_time::usec( 60000 + 33 ) );

}

/*
 * Once the link layer received a PDU from the master, the supervision timeout is in charge
 * In this example, the timeout is 720ms, the connection interval is 30ms, so the timeout is
 * reached after 24 connection intervals (that's the 25th schedule request; plus the first one after the con request).
 */
BOOST_FIXTURE_TEST_CASE( supervision_timeout_is_in_charge, only_one_pdu_from_master )
{
    BOOST_CHECK_EQUAL( connection_events().size(), 26u );
}

template < std::uint16_t Instance = 6, std::uint64_t Map = 0x1555555555, unsigned EmptyPDUs = Instance + 2 >
struct setup_connect_and_channel_map_request_base : unconnected
{
    static_assert( Instance > 0, "Instance > 0" );

    void channel_map_request( std::uint16_t instance, std::uint64_t map )
    {
        ll_control_pdu( {
            0x01,                                                   // opcode
            static_cast< std::uint8_t >( map >> 0 ),                // map
            static_cast< std::uint8_t >( map >> 8 ),
            static_cast< std::uint8_t >( map >> 16 ),
            static_cast< std::uint8_t >( map >> 24 ),
            static_cast< std::uint8_t >( map >> 32 ),
            static_cast< std::uint8_t >( instance >> 0 ),           // instance
            static_cast< std::uint8_t >( instance >> 8 )
        } );
    }

    setup_connect_and_channel_map_request_base()
    {
        respond_to( 37, valid_connection_request_pdu );
        channel_map_request( Instance, Map );


        for ( unsigned empty = 0; empty != EmptyPDUs; ++empty )
        {
            ll_empty_pdu();
        }
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

// static const auto filter_channel_map_requests = []( const test::connection_event& ev ) -> bool
// {
//     if ( ev.received_data.empty() )
//         return false;

//     const auto& pdu = ev.received_data.front();

//     return pdu.size() > 2 && ( pdu[ 0 ] & 0x03 ) == 0x03 && pdu[ 2 ] == 0x01;
// };

/*
 * When the instance is in the past, the slave should consider the connection to be lost.
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
        0x01,                                                   // opcode
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

    auto response = connection_events().at( 1u ).transmitted_data.front();
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
    auto pdu = connection_events().at( 10 ).transmitted_data.front();
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

/*
 * This test should make sure the instance is correctly interpreted after the connect count wrapped from 0xffff to 0x0000
 */
BOOST_FIXTURE_TEST_CASE( channel_map_request_after_connection_count_wrap, connect_and_channel_map_request )
{

}

BOOST_FIXTURE_TEST_CASE( l2cap_data_during_channel_map_request_with_buffer_big_enough_for_two_pdus, only_one_pdu_from_master )
{
}

BOOST_FIXTURE_TEST_CASE( l2cap_data_during_channel_map_request_with_buffer_big_enough_for_only_one_pdus, only_one_pdu_from_master )
{
}
