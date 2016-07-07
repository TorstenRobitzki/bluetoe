#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/l2cap_signaling_channel.hpp>

#include "connected.hpp"

using namespace test;

/*
 * To request the update of the connection parameters, a peripheral / link layer slave
 * has two options:
 * - LL Connection Parameters Request Procedure (ll)
 * - L2CAP Connection Parameter Update Request (l2cap)
 *
 * Where the standard prefers the the former, if both, slave and master supports
 * that procedure.
 *
 * So, to pick one of them, its crucial to know whether the master supports the
 * LL Connection Parameters Request Procedure.
 *
 * Lets start by assuming, that the master implements the LL Connection Parameters Request Procedure.
 * If a LL_VERSION_IND is received from the master indicating, that the masters link layer version
 * is 4.0 (or less), then the master does not implement LL Connection Parameters Request Procedure.
 * If a LL_FEATURE_REQ PDU is received, it's clear whether the master implements the
 * LL Connection Parameters Request Procedure or not.
 *
 * LL Connection Parameters Request Procedure should be optional, so if the slave votes to not implement
 * LL Connection Parameters Request Procedure, than we should default to l2cap.
 */

struct link_layer_with_signaling_channel : unconnected_base< bluetoe::l2cap::signaling_channel<> >
{
    link_layer_with_signaling_channel()
    {
        respond_to( 37, valid_connection_request_pdu );
    }
};

BOOST_FIXTURE_TEST_CASE( if_master_protocol_version_is_unknown_try_ll, link_layer_with_signaling_channel )
{
}

BOOST_FIXTURE_TEST_CASE( if_master_protocol_version_is_40_use_l2cap, link_layer_with_signaling_channel )
{
    ll_control_pdu(
        {
            0x03, 0x06,
            0x0C,               // LL_VERSION_IND
            0x06,               // VersNr = Core Specification 4.0
            0x00, 0x02,         // CompId
            0x00, 0x00          // SubVersNr
        } );

    add_connection_event_respond(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_l2cap_pdu( {
        X,  X, 0x05, 0x00, 0x12, and_so_on
    } );
}

BOOST_FIXTURE_TEST_CASE( if_masters_features_dont_contain_ll_use_l2cap, link_layer_with_signaling_channel )
{
}

BOOST_FIXTURE_TEST_CASE( if_masters_features_contain_ll_use_ll, link_layer_with_signaling_channel )
{
}

BOOST_FIXTURE_TEST_CASE( if_ll_doesn_work_fallback_to_l2cap, link_layer_with_signaling_channel )
{
}

BOOST_FIXTURE_TEST_CASE( if_ll_doesn_work_start_by_using_l2cap, link_layer_with_signaling_channel )
{
}
