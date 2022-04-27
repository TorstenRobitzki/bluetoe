#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

BOOST_FIXTURE_TEST_CASE( respond_with_an_unknown_rsp, unconnected )
{
    check_single_ll_control_pdu(
        { 0x03, 0x01, 0xff },
        {
            0x03, 0x02,
            0x07, 0xff
        },
        "respond_with_an_unknown_rsp"
    );
}

BOOST_FIXTURE_TEST_CASE( responding_in_feature_setup, unconnected )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_version_ind, unconnected )
{
    check_single_ll_control_pdu(
        {
            0x03, 0x06,
            0x0C,               // LL_VERSION_IND
            0x08,               // VersNr = Core Specification 4.2
            0x00, 0x02,         // CompId
            0x00, 0x00          // SubVersNr
        },
        {
            0x03, 0x06,
            0x0C,               // LL_VERSION_IND
            0x09,               // VersNr = Core Specification 5.0
            0x69, 0x02,         // CompId
            0x00, 0x00          // SubVersNr
        },
        "respond_to_a_version_ind"
    );
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_ping, unconnected )
{
    check_single_ll_control_pdu(
        {
            0x03, 0x01,
            0x12                // LL_PING_REQ
        },
        {
            0x03, 0x01,
            0x13                // LL_PING_RSP
        },
        "respond_to_a_ping"
    );
}

BOOST_FIXTURE_TEST_CASE( starts_advertising_after_termination, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_event_respond(
        {
            0x03, 0x02,
            0x02, 0x12
        } );

    run();

    // the second advertising PDU is the response to the terminate PDU
    // and must happen much earlier than the supervision timeout
    BOOST_REQUIRE_GT( advertisings().size(), 1u );
    const auto& second_advertisment = advertisings()[ 1 ];

    BOOST_CHECK_LT( second_advertisment.on_air_time, bluetoe::link_layer::delta_time::msec( 50 ) );
}

BOOST_FIXTURE_TEST_CASE( do_not_respond_to_UNKNOWN_RSP, unconnected )
{
    check_single_ll_control_pdu(
        {
            0x03, 0x02,
            0x07,               // LL_UNKNOWN_RSP
            0x07                // request opcode
        },
        {
            0x01, 0x00
        },
        "do_not_respond_to_UNKNOWN_RSP"
    );
}

BOOST_FIXTURE_TEST_CASE( do_not_respond_to_UNKNOWN_RSP_even_if_broken, unconnected )
{
    check_single_ll_control_pdu(
        {
            0x03, 0x03,
            0x07,               // LL_UNKNOWN_RSP
            0x07,               // request opcode
            0xff                // additional byte
        },
        {
            0x01, 0x00
        },
        "do_not_respond_to_UNKNOWN_RSP"
    );
}

/*
 * Example from core spec Vol6, Part B, 5.1.2
 */
BOOST_FIXTURE_TEST_CASE( Channel_Map_Update_procedure, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );

    ll_control_pdu(
        {
            0x01,                           // LL_CHANNEL_MAP_IND
            0xff, 0xf7, 0xff, 0xff, 0x1f,   // new channel map: all channels enabled except channel 11
            0x64, 0x00                      // Instance: 100dez
        }
    );

    ll_empty_pdus( 101 );

    run();

    BOOST_CHECK_EQUAL( connection_events()[ 99 ].channel, 1u );
    BOOST_CHECK_EQUAL( connection_events()[ 100 ].channel, 12u );
    BOOST_CHECK_EQUAL( connection_events()[ 101 ].channel, 21u );
}

BOOST_FIXTURE_TEST_CASE( Channel_Map_Update_procedure_With_Latency, unconnected )
{
    respond_to( 37, {
        0xc5, 0x22,                         // header
        0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
        0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
        0x08, 0x81, 0xf6,                   // CRC Init
        0x03,                               // transmit window size
        0x0b, 0x00,                         // window offset
        0x18, 0x00,                         // interval (30ms)
        0x28, 0x00,                         // peripheral latency 40
        0x48, 0x01,                         // connection timeout (3280ms)
        0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
        0xaa                                // hop increment and sleep clock accuracy (10 and 50ppm)
    } );

    ll_control_pdu(
        {
            0x01,                           // LL_CHANNEL_MAP_IND
            0xff, 0xf7, 0xff, 0xff, 0x1f,   // new channel map: all channels enabled except channel 11
            0x64, 0x00                      // Instance: 100dez
        }
    );

    ll_empty_pdus( 5 );

    run();

    // First event at instant 0; second at 41, third at 82; 4th at 100
    BOOST_CHECK_EQUAL( connection_events()[ 3 ].channel, 12u );
}
