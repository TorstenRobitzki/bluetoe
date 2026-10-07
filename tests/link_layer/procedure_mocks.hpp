#ifndef BLUETOE_TESTS_LINK_LAYER_PROCEDURE_MOCKS_HPP
#define BLUETOE_TESTS_LINK_LAYER_PROCEDURE_MOCKS_HPP

#include <boost/test/unit_test.hpp>

#include <bluetoe/buffer.hpp>
#include <bluetoe/connection_parameters.hpp>
#include <bluetoe/procedures.hpp>
#include <bluetoe/phy_encodings.hpp>
#include <bluetoe/security_connection_data.hpp>

#include "test_layout.hpp"
#include "procedures_io.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <utility>
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

    // the PHYs of a PHY update, applied at its instant; le_unchanged_coding keeps a direction
    struct phy_update
    {
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t c_to_p;
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t p_to_c;

        bool operator==( const phy_update& ) const = default;
    };

    template < class LinkData >
    void update_phy( LinkData&,
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t c_to_p,
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t p_to_c )
    {
        phy_updates.push_back( { c_to_p, p_to_c } );
    }

    std::vector< bluetoe::link_layer::details::connection_timing >  connection_updates;
    std::vector< std::array< std::uint8_t, 5 > >                    channel_map_updates;
    std::vector< phy_update >                                       phy_updates;

    // the source of keys: the long term key for EDIV and Rand, if there is one
    bool                                                        has_key = true;
    bluetoe::details::uint128_t                                 key     = {{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 }};
    std::vector< std::pair< std::uint16_t, std::uint64_t > >    key_requests;

    template < class LinkData >
    std::pair< bool, bluetoe::details::uint128_t > find_key( LinkData&, std::uint16_t ediv, std::uint64_t rand )
    {
        key_requests.push_back( { ediv, rand } );

        return { has_key, key };
    }

    // the radio's part of the session setup: it derives the session key and returns SKDs and IVs
    struct encryption_setup
    {
        bluetoe::details::uint128_t key;
        std::uint64_t               skdm;
        std::uint32_t               ivm;

        bool operator==( const encryption_setup& ) const = default;
    };

    std::uint64_t                   skds = 0x5857565554535251;
    std::uint32_t                   ivs  = 0x64636261;
    std::vector< encryption_setup > encryption_setups;

    template < class LinkData >
    std::pair< std::uint64_t, std::uint32_t > setup_encryption( LinkData&, const bluetoe::details::uint128_t& session_key, std::uint64_t skdm, std::uint32_t ivm )
    {
        encryption_setups.push_back( { session_key, skdm, ivm } );

        return { skds, ivs };
    }

    // what the radio decrypts and encrypts
    bool receive_encrypted  = false;
    bool transmit_encrypted = false;

    template < class LinkData >
    bool encrypt_receive( LinkData&, bool encrypted )
    {
        bool result = receive_encrypted != encrypted;
        receive_encrypted = encrypted;

        return result;
    }

    template < class LinkData >
    bool encrypt_transmit( LinkData&, bool encrypted )
    {
        bool result = transmit_encrypted != encrypted;
        transmit_encrypted = encrypted;

        return result;
    }

    // the sink of the encryption state of the link: the callbacks, the restored CCCDs
    std::vector< bool > encryption_changes;

    template < class LinkData >
    void encryption_changed( LinkData&, bool encrypted )
    {
        encryption_changes.push_back( encrypted );
    }
};

// we simulate the situation where a connection is established, so we need a valid
// channel map (indeed the hop is important)
inline const std::array< std::uint8_t, 5 > init_map_data({{ 0xFF, 0xFF, 0xFF, 0xFF, 0x1F }});

// the link data of a link that runs the procedures of the procedure_list<> Procedures
template < class Procedures >
struct link_data_mock : Procedures::state_type
{
    using procedures = Procedures;

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

/*
 * The link layer calls connection_event() before it sets up an event, with the counter of
 * that event. This runs the events after the current one, up to and including last.
 */
template < class LinkData >
void run_connection_events( link_layer_mock& link_layer, LinkData& link, std::uint16_t last )
{
    while ( link.connection_event_counter_value != last )
    {
        ++link.connection_event_counter_value;

        const auto result = LinkData::procedures::connection_event( link_layer, link );
        BOOST_TEST( result == bluetoe::link_layer::details::procedure_result::handled() );
    }
}

#endif
