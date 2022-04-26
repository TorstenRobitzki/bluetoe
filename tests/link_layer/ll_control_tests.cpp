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
