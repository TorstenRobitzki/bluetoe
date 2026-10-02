#ifndef BLUETOE_TESTS_LINK_LAYER_PROCEDURE_MOCKS_HPP
#define BLUETOE_TESTS_LINK_LAYER_PROCEDURE_MOCKS_HPP

#include <boost/test/unit_test.hpp>

#include <bluetoe/buffer.hpp>
#include <bluetoe/connection_parameters.hpp>

#include "test_layout.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

/*
 * A gap between header and payload: a procedure that does not go through the layout
 * reads and writes the wrong octets.
 */
using pdu_layout = test::layout_with_overhead< 2 >;

// the LLID of an LL control PDU
constexpr std::uint16_t ll_control_pdu_llid = 0x03;

// an LL control PDU in the layout, as the radio stores it
inline std::vector< std::uint8_t > control_pdu( std::initializer_list< std::uint8_t > payload )
{
    std::vector< std::uint8_t >             pdu( pdu_layout::data_channel_pdu_memory_size( payload.size() ) );
    const bluetoe::link_layer::read_buffer  buffer{ pdu.data(), pdu.size() };

    pdu_layout::header( buffer, ll_control_pdu_llid | ( payload.size() << 8 ) );
    std::copy( payload.begin(), payload.end(), pdu_layout::body( buffer ).first );

    return pdu;
}

inline std::span< const std::uint8_t > payload( const std::vector< std::uint8_t >& pdu )
{
    return pdu_layout::payload( bluetoe::link_layer::write_buffer{ pdu.data(), pdu.size() } );
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

    bluetoe::link_layer::read_buffer allocate_ll_transmit_buffer( std::size_t payload_size )
    {
        const std::size_t size = layout::data_channel_pdu_memory_size( payload_size );

        if ( !room_for_an_answer || size > buffer_.size() )
            return { nullptr, 0 };

        allocated_ = { buffer_.data(), size };

        return allocated_;
    }

    void commit_ll_transmit_buffer( bluetoe::link_layer::read_buffer pdu )
    {
        BOOST_REQUIRE( allocated_.buffer != nullptr );
        BOOST_REQUIRE( pdu.buffer == allocated_.buffer );
        BOOST_REQUIRE_EQUAL( pdu.size, allocated_.size );

        // the size of an answer is known before it is written, so exactly that much is allocated
        const std::size_t size = layout::data_channel_pdu_memory_size( layout::header( pdu ) >> 8 );
        BOOST_REQUIRE_EQUAL( size, allocated_.size );

        transmitted.emplace_back( pdu.buffer, pdu.buffer + size );
        allocated_ = { nullptr, 0 };
    }

    bool                                        room_for_an_answer = true;
    std::vector< std::vector< std::uint8_t > >  transmitted;

private:
    std::array< std::uint8_t, 64 >              buffer_ = {};
    bluetoe::link_layer::read_buffer            allocated_ = { nullptr, 0 };
};

/*
 * Part of the interface of the link_layer, that is required by the procedure_list
 */
struct link_layer_mock
{
    std::uint8_t supported_link_layer_version() const
    {
        return 0x09;
    }

    std::uint16_t link_layer_company_identifier() const
    {
        return 0x0269;
    }

    // the new timing of a connection update, applied at its instant
    template < class LinkData >
    void update_connection( LinkData& link, const bluetoe::link_layer::details::connection_timing& timing )
    {
        connection_updates.push_back( timing );
        link.parameters.update_timing( timing );
    }

    template < class LinkData >
    void update_channel_map( LinkData& link, std::array< std::uint8_t, 5 > new_map )
    {
        channel_map_updates.push_back( new_map );
        link.parameters.channels( new_map.data() );
    }

    std::vector< bluetoe::link_layer::details::connection_timing >  connection_updates;
    std::vector< std::array< std::uint8_t, 5 > >                    channel_map_updates;
};

// we simulate the situation where a connection is established, so we need a valid
// channel map (indeed the hop is important)
inline const std::array< std::uint8_t, 5 > init_map_data({{ 0xFF, 0xFF, 0xFF, 0xFF, 0x1F }});

// the link data of a link that runs the procedures of the procedure_list<> Procedures
template < class Procedures >
struct link_data_mock : Procedures::state_type
{
    link_data_mock()
    {
        parameters.channels( init_map_data.data() );
    }

    buffers_mock                                        buffers;
    bluetoe::link_layer::details::connection_parameters parameters;

    // the event in which a PDU is received, or that the link layer is about to set up
    std::uint16_t   connection_event_counter_value = 0;

    std::uint16_t connection_event_counter()
    {
        return connection_event_counter_value;
    }
};

#endif
