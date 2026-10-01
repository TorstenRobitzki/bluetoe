#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedures_io.hpp"

namespace bll = bluetoe::link_layer;

namespace {

    using le_ping           = bll::details::procedure_list< bll::details::le_ping_procedure >;
    using version_exchange  = bll::details::procedure_list< bll::details::version_exchange_procedure >;
    using termination       = bll::details::procedure_list< bll::details::termination_procedure >;

    const auto ping_req = control_pdu( {
        0x12                // LL_PING_REQ
    } );

    const auto version_ind = control_pdu( {
        0x0C,               // LL_VERSION_IND
        0x0D,               // VersNr: Core 5.4
        0x59, 0x00,         // CompId: Nordic Semiconductor
        0x34, 0x12          // SubVersNr
    } );

    const auto own_version_ind = control_pdu( {
        0x0C,               // LL_VERSION_IND
        0x09,               // VersNr: Core 5.0
        0x69, 0x02,         // CompId: 0x0269
        0x00, 0x00          // SubVersNr
    } );

    const auto terminate_ind = control_pdu( {
        0x02,               // LL_TERMINATE_IND
        0x13                // ErrorCode: Remote User Terminated Connection
    } );
}

/*
 * LE Ping procedure
 */
BOOST_AUTO_TEST_CASE( a_ping_request_is_answered_with_a_ping_response )
{
    link_layer_mock             link_layer;
    link_data_mock< le_ping >   link;

    const auto result = le_ping::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto ping_rsp = control_pdu( {
        0x13                // LL_PING_RSP
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == ping_rsp );
}

BOOST_AUTO_TEST_CASE( a_ping_request_stalls_without_room_for_the_response )
{
    link_layer_mock             link_layer;
    link_data_mock< le_ping >   link;

    link.buffers.room_for_an_answer = false;

    const auto result = le_ping::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * Version Exchange procedure
 */
BOOST_AUTO_TEST_CASE( a_version_indication_is_answered_with_the_own_version )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

BOOST_AUTO_TEST_CASE( a_version_indication_stalls_without_room_for_the_answer )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    link.buffers.room_for_an_answer = false;

    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * A stall must not count as answered: the indication is passed again at the next connection
 * event, and then it is answered.
 */
BOOST_AUTO_TEST_CASE( a_stalled_version_indication_is_answered_when_there_is_room )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    link.buffers.room_for_an_answer = false;
    version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    link.buffers.room_for_an_answer = true;
    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

/*
 * Core Vol 6, Part B, 5.1.5: a link layer sends at most one LL_VERSION_IND per connection.
 */
BOOST_AUTO_TEST_CASE( a_second_version_indication_is_not_answered )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );
    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.size() == 1u );
}

/*
 * ACL Termination procedure, started by the peer
 */
BOOST_AUTO_TEST_CASE( a_terminate_indication_disconnects_with_the_peers_error_code )
{
    link_layer_mock                 link_layer;
    link_data_mock< termination >   link;

    const auto result = termination::handle_control_pdu( link_layer, link, payload( terminate_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( 0x13 ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// there is no answer to a terminate indication, so a full transmit buffer does not stall it
BOOST_AUTO_TEST_CASE( a_terminate_indication_disconnects_without_room_for_an_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< termination >   link;

    link.buffers.room_for_an_answer = false;

    const auto result = termination::handle_control_pdu( link_layer, link, payload( terminate_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( 0x13 ) );
}
