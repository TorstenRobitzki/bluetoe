#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>
#include <bluetoe/buffer.hpp>

#include "test_layout.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

namespace bll = bluetoe::link_layer;

namespace {

    /*
     * A gap between header and payload: a procedure that does not go through the layout
     * reads and writes the wrong octets.
     */
    using pdu_layout = test::layout_with_overhead< 2 >;

    // the LLID of an LL control PDU
    constexpr std::uint16_t ll_control_pdu_llid = 0x03;

    // an LL control PDU in the layout, as the radio stores it
    std::vector< std::uint8_t > control_pdu( std::initializer_list< std::uint8_t > payload )
    {
        std::vector< std::uint8_t > pdu( pdu_layout::data_channel_pdu_memory_size( payload.size() ) );
        const bll::read_buffer      buffer{ pdu.data(), pdu.size() };

        pdu_layout::header( buffer, ll_control_pdu_llid | ( payload.size() << 8 ) );
        std::copy( payload.begin(), payload.end(), pdu_layout::body( buffer ).first );

        return pdu;
    }

    std::span< const std::uint8_t > payload( const std::vector< std::uint8_t >& pdu )
    {
        return pdu_layout::payload( bll::write_buffer{ pdu.data(), pdu.size() } );
    }

    /*
     * The link data's buffers as the procedures use them, the interface of
     * ll_l2cap_sdu_buffer<>: room for one LL PDU in the layout, or none, and a record of
     * what was committed. Like the real buffer, a commit takes the PDU's length from its
     * header, and only a buffer handed out by the last allocation can be committed; its
     * length has to fill that allocation.
     */
    struct buffers_mock
    {
        using layout = pdu_layout;

        bll::read_buffer allocate_ll_transmit_buffer( std::size_t payload_size )
        {
            const std::size_t size = layout::data_channel_pdu_memory_size( payload_size );

            if ( !room_for_an_answer || size > buffer_.size() )
                return { nullptr, 0 };

            allocated_ = { buffer_.data(), size };

            return allocated_;
        }

        void commit_ll_transmit_buffer( bll::read_buffer pdu )
        {
            BOOST_REQUIRE( allocated_.buffer != nullptr );
            BOOST_REQUIRE( pdu.buffer == allocated_.buffer );
            BOOST_REQUIRE_EQUAL( pdu.size, allocated_.size );

            // the list knows the size of its answers, so it allocates exactly that much
            const std::size_t size = layout::data_channel_pdu_memory_size( layout::header( pdu ) >> 8 );
            BOOST_REQUIRE_EQUAL( size, allocated_.size );

            transmitted.emplace_back( pdu.buffer, pdu.buffer + size );
            allocated_ = { nullptr, 0 };
        }

        bool                                        room_for_an_answer = true;
        std::vector< std::vector< std::uint8_t > >  transmitted;

    private:
        std::array< std::uint8_t, 64 >              buffer_ = {};
        bll::read_buffer                            allocated_ = { nullptr, 0 };
    };

    // nothing of the link layer is used yet
    struct link_layer_mock {
        using layout_t = pdu_layout;
    };

    using no_procedures = bll::details::procedure_list<>;

    struct link_data_mock : no_procedures::state_type
    {
        buffers_mock buffers;
    };

    const auto ping_req = control_pdu( {
        0x12                // LL_PING_REQ
    } );
}

BOOST_AUTO_TEST_CASE( an_empty_list_answers_any_request_as_unknown )
{
    link_layer_mock link_layer;
    link_data_mock  link;

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_CHECK( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( an_unknown_request_stalls_without_room_for_the_answer )
{
    link_layer_mock link_layer;
    link_data_mock  link;

    link.buffers.room_for_an_answer = false;

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_CHECK( result == bll::details::procedure_result::stalled() );
    BOOST_CHECK( link.buffers.transmitted.empty() );
}
/*
 * A stalled PDU stays in the receive buffer, and the link layer passes it again at the next
 * connection event; once there is room, it is answered, once.
 */
BOOST_AUTO_TEST_CASE( a_stalled_request_is_answered_when_there_is_room )
{
    link_layer_mock link_layer;
    link_data_mock  link;

    link.buffers.room_for_an_answer = false;

    const auto stalled = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_CHECK( stalled == bll::details::procedure_result::stalled() );
    BOOST_CHECK( link.buffers.transmitted.empty() );

    link.buffers.room_for_an_answer = true;

    const auto handled = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_CHECK( handled == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}
