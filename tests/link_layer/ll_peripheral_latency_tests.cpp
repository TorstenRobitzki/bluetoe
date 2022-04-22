#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

std::uint16_t temperature_value = 0;

// We test with a relative small hop of 5 for easier calculations
static const std::initializer_list< std::uint8_t > five_hop_connection_request_pdu =
{
    0xc5, 0x22,                         // header
    0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
    0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
    0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
    0x08, 0x81, 0xf6,                   // CRC Init
    0x03,                               // transmit window size
    0x0b, 0x00,                         // window offset
    0x18, 0x00,                         // interval (30ms)
    0x03, 0x00,                         // peripheral latency
    0x48, 0x00,                         // connection timeout (720ms)
    0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
    0xa5                                // hop increment and sleep clock accuracy (5 and 50ppm)
};

using server_t = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
            bluetoe::no_write_access,
            bluetoe::notify
        >
    >,
    bluetoe::no_gap_service_for_gatt_servers
>;

/*
 * Currently, there is no data to distingusish between Data Available and Unacknowledged Data Available,
 * so lets defere, what to do about that and just test with both options set
 */
struct fixture_with_listen_if_pending_transmit_data_option
    : unconnected_base_t<
        server_t,
        test::radio,
        bluetoe::link_layer::peripheral_latency_configuration<
            bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
            bluetoe::link_layer::peripheral_latency::listen_if_unacknowledged_data
        >
    >
{
};

BOOST_FIXTURE_TEST_CASE( planned_connection_event_is_rescheduled_when_l2cap_layer_generates_pending_data, fixture_with_listen_if_pending_transmit_data_option )
{
    respond_to( 37, five_hop_connection_request_pdu );

    ll_data_pdu( {
        0x05, 0x00,         // length
        0x04, 0x00,         // Channel
        0x12,               // ATT_WRITE_REQ PDU
        0x04, 0x00,         // CCCD handle
        0x01, 0x00          // Enable notifications
    } );
    ll_empty_pdu();
    ll_empty_pdu();
    ll_empty_pdu();

    run();

    BOOST_REQUIRE( connection_events().size() >= 5u );

    // the first connection event happens on channel
    BOOST_TEST( connection_events()[ 0 ].channel == 5u );

    // the second connection event happens at channel 10, as the respond to the ATT_WRITE_REQ
    // is available to be send out
    BOOST_TEST( connection_events()[ 1 ].channel == 10u );

    // the third connection event happens at channel 15, as the outgoing data was not acknowledged
    BOOST_TEST( connection_events()[ 2 ].channel == 15u );

    // the forth connection event happens at channel 15 + 4 * 5 == 35u, as the next connection
    // event contains no data
    BOOST_TEST( connection_events()[ 3 ].channel == 35u );
}

BOOST_FIXTURE_TEST_CASE( timeout, fixture_with_listen_if_pending_transmit_data_option )
{
    respond_to( 37, five_hop_connection_request_pdu );

    // 3 connection events, the first empty, the second, a timeout and the last, empty again
    ll_empty_pdu();
    add_connection_event_respond_timeout();
    ll_empty_pdu();

    run();

    BOOST_REQUIRE( connection_events().size() >= 3u );

    // the first connection event happens on channel 5
    BOOST_TEST( connection_events()[ 0 ].channel == 5u );

    // the second connection event happens at channel 25 due to the peripheral latency
    BOOST_TEST( connection_events()[ 1 ].channel == 25u );

    // the third connection event happens at channel 30, due to the timeout
    BOOST_TEST( connection_events()[ 2 ].channel == 30u );
}

/*
 * This test makes sure, that the instance at which the next connection parameters
 * become valid will be taken
 */
BOOST_FIXTURE_TEST_CASE( connection_parameter_update, fixture_with_listen_if_pending_transmit_data_option )
{
    respond_to( 37, five_hop_connection_request_pdu );
    ll_empty_pdu();
    add_connection_update_request(
        6, 3, 6,
        // latency: 66, timeout: 198, instance: 10
        66, 198, 10 );
    add_empty_pdus( 3 );

    run();

    // first connection event at channel 5

    BOOST_REQUIRE( connection_events().size() >= 4u );

    // the first connection event happens on channel 5 (instance = 0)
    BOOST_TEST( connection_events()[ 0 ].channel == 5u );

    // the second connection event happens at channel 25 due to the peripheral latency (instance = 4)
    BOOST_TEST( connection_events()[ 1 ].channel == 25u );

    // third connection event happens at 25 + 4 * 5 due to peripheral latency (instance = 8)
    BOOST_TEST( connection_events()[ 2 ].channel == 45u % 37u );

    // 4th connection event happens instance == 10
    BOOST_TEST( connection_events()[ 3 ].channel == 55u % 37u );
}

using config1 = bluetoe::link_layer::peripheral_latency_configuration<>;
using config2 = bluetoe::link_layer::peripheral_latency_configuration<
                    bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data,
                    bluetoe::link_layer::peripheral_latency::listen_if_unacknowledged_data
                >;

struct fixture_with_runtime_configuration
    : unconnected_base_t<
        server_t,
        test::radio,
        bluetoe::link_layer::peripheral_latency_configuration_set< config1, config2 >
    >
{
};

BOOST_FIXTURE_TEST_CASE( change_peripheral_configuration_at_runtime, fixture_with_runtime_configuration )
{
    this->respond_to( 37, five_hop_connection_request_pdu );

    ll_data_pdu( {
        0x05, 0x00,         // length
        0x04, 0x00,         // Channel
        0x12,               // ATT_WRITE_REQ PDU
        0x04, 0x00,         // CCCD handle
        0x01, 0x00          // Enable notifications
    } );
    ll_empty_pdu();
    ll_function_call( [&](){
        change_peripheral_latency< config2 >();
    } );
    ll_data_pdu( {
        0x05, 0x00,         // length
        0x04, 0x00,         // Channel
        0x12,               // ATT_WRITE_REQ PDU
        0x04, 0x00,         // CCCD handle
        0x00, 0x00          // Disable notifications
    } );
    ll_empty_pdu();

    this->run();

    // the first connection event happens on channel 5
    BOOST_TEST( connection_events()[ 0 ].channel == 5u );

    // the second and third connection event happens at channel 25, despide the fact, that
    // outgoing data is present.
    BOOST_TEST( connection_events()[ 1 ].channel == 25u );
    BOOST_TEST( connection_events()[ 2 ].channel == 45u % 37u);

    // Now switching configurations to take next possible connection event, if outgoing data is available
    BOOST_TEST( connection_events()[ 3 ].channel == 65u % 37u);

    BOOST_TEST( connection_events()[ 4 ].channel == 70u % 37u);
    BOOST_TEST( connection_events()[ 5 ].channel == 75u % 37u);
}
