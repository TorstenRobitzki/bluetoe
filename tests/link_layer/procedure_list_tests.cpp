#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedures_io.hpp"

#include <cstdint>
#include <span>

namespace bll = bluetoe::link_layer;

namespace {

    class fixture_procedure
    {
    public:
        // a made up opcode
        static constexpr std::uint8_t opcode        = 0xF0;
        static constexpr std::uint8_t ctr_data_size = 4;

        template < class LinkLayer, class LinkData >
        static bll::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            ++link.side_effect;

            return bll::details::procedure_result::handled();
        }

        struct state_type {};
        struct instant_state {};
    };

    using no_procedures = bll::details::procedure_list<>;

    using single_procedure = bll::details::procedure_list< fixture_procedure >;

    struct single_procedure_link_data_mock : link_data_mock< single_procedure >
    {
        int side_effect = 0;
    };

    const auto ping_req = control_pdu( {
        0x12                // LL_PING_REQ
    } );
}

BOOST_AUTO_TEST_CASE( an_empty_list_answers_any_request_as_unknown )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( an_unknown_request_stalls_without_room_for_the_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    link.buffers.room_for_an_answer = false;

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * A stalled PDU stays in the receive buffer, and the link layer passes it again at the next
 * connection event; once there is room, it is answered, once.
 */
BOOST_AUTO_TEST_CASE( a_stalled_request_is_answered_when_there_is_room )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    link.buffers.room_for_an_answer = false;

    const auto stalled = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( stalled == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    link.buffers.room_for_an_answer = true;

    const auto handled = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( handled == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

/*
 * An LL control PDU shall not have a length of 0. There is no opcode to report back, and the
 * connection has to keep being served, so such a PDU is ignored.
 */
BOOST_AUTO_TEST_CASE( a_control_pdu_without_an_opcode_is_ignored )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    const auto no_opcode = control_pdu( {} );

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( no_opcode ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * Ignoring needs no answer, so a PDU without an opcode never stalls the PDUs behind it.
 */
BOOST_AUTO_TEST_CASE( a_control_pdu_without_an_opcode_is_ignored_without_room_for_an_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    link.buffers.room_for_an_answer = false;

    const auto no_opcode = control_pdu( {} );

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( no_opcode ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( a_known_opcode_is_passed_to_its_procedure )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    const auto request = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_TEST( link.side_effect == 1 );
    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( known_opcode_but_request_too_long )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    // the correct size would be one opcode and 4 bytes of CtrData
    const auto invalid_size = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04, 0x05 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( invalid_size ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.side_effect == 0 );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,                       // LL_UNKNOWN_RSP
        fixture_procedure::opcode   // UnknownType: fixture_procedure::opcode
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( known_opcode_but_request_too_small )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    // the correct size would be one opcode and 4 bytes of CtrData
    const auto invalid_size = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( invalid_size ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.side_effect == 0 );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,                       // LL_UNKNOWN_RSP
        fixture_procedure::opcode   // UnknownType: fixture_procedure::opcode
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( known_opcode_but_request_too_long_without_room_for_an_answer )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    link.buffers.room_for_an_answer = false;

    const auto invalid_size = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04, 0x05 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( invalid_size ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.side_effect == 0 );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( after_a_link_got_reset_all_link_state_is_reset_too )
{
    // TODO
}