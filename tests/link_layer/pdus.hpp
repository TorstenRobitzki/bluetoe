#ifndef BLUETOE_TESTS_LINK_LAYER_PDUS_HPP
#define BLUETOE_TESTS_LINK_LAYER_PDUS_HPP

/**
 * @file pdus.hpp
 *
 * The PDUs the link layer tests send and expect, built by name with their fields as
 * parameters, so that a test spells out only the PDU it is about. Every builder returns
 * the bytes as they are on air; the ones of link layer control PDUs return the payload
 * behind the data channel header, which is what ll_control_pdu() takes, and ll_control()
 * and ll_empty() put the header in front where a whole PDU is needed.
 */

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace test {

    using bytes_t = std::vector< std::uint8_t >;

    /**
     * @brief the LLIDs of a data channel PDU
     */
    namespace llid {
        constexpr std::uint8_t continuation = 0x01;
        constexpr std::uint8_t start        = 0x02;
        constexpr std::uint8_t control      = 0x03;
    }

    inline std::uint8_t lo( std::uint32_t value )
    {
        return static_cast< std::uint8_t >( value );
    }

    inline std::uint8_t hi( std::uint32_t value )
    {
        return static_cast< std::uint8_t >( value >> 8 );
    }

    /**
     * @brief a data channel PDU with `llid`, SN and NESN clear, around `payload`
     */
    inline bytes_t ll_pdu( std::uint8_t llid, const bytes_t& payload )
    {
        bytes_t result = { llid, static_cast< std::uint8_t >( payload.size() ) };
        result.insert( result.end(), payload.begin(), payload.end() );

        return result;
    }

    inline bytes_t ll_control( const bytes_t& payload )
    {
        return ll_pdu( llid::control, payload );
    }

    inline bytes_t ll_empty()
    {
        return ll_pdu( llid::continuation, {} );
    }

    /**
     * @brief the parameters of a CONNECT_IND, the defaults being the connection the tests
     *        run on: a 30 ms interval, no latency, a 720 ms timeout, all channels
     */
    struct connection_parameters
    {
        std::uint8_t    window_size     = 0x03;
        std::uint16_t   window_offset   = 0x000b;
        std::uint16_t   interval        = 0x0018;
        std::uint16_t   latency         = 0x0000;
        std::uint16_t   timeout         = 0x0048;
        std::uint64_t   channel_map     = 0x1fffffffff;
        std::uint8_t    hop             = 10;
        std::uint8_t    sleep_clock_accuracy = 5;
    };

    /**
     * @brief a CONNECT_IND from the initiator 48:f0:92:62:1c:3c to the advertiser
     *        c0:0f:15:08:11:47, both random, on access address 0xaf9ab35a
     */
    inline bytes_t connect_ind( const connection_parameters& p = {} )
    {
        return {
            0xc5, 0x22,                         // header: CONNECT_IND, TxAdd and RxAdd random, 34 bytes
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            p.window_size,                      // transmit window size
            lo( p.window_offset ), hi( p.window_offset ),
            lo( p.interval ), hi( p.interval ),
            lo( p.latency ), hi( p.latency ),
            lo( p.timeout ), hi( p.timeout ),
            static_cast< std::uint8_t >( p.channel_map ),        // used channel map
            static_cast< std::uint8_t >( p.channel_map >> 8 ),
            static_cast< std::uint8_t >( p.channel_map >> 16 ),
            static_cast< std::uint8_t >( p.channel_map >> 24 ),
            static_cast< std::uint8_t >( p.channel_map >> 32 ),
            static_cast< std::uint8_t >( p.hop | ( p.sleep_clock_accuracy << 5 ) )
        };
    }

    /**
     * @name link layer control PDUs, the payload behind the data channel header
     * @{
     */
    inline bytes_t ll_connection_update_ind(
        std::uint8_t window_size, std::uint16_t window_offset, std::uint16_t interval,
        std::uint16_t latency, std::uint16_t timeout, std::uint16_t instant )
    {
        return {
            0x00,                                   // LL_CONNECTION_UPDATE_IND
            window_size,
            lo( window_offset ), hi( window_offset ),
            lo( interval ), hi( interval ),
            lo( latency ), hi( latency ),
            lo( timeout ), hi( timeout ),
            lo( instant ), hi( instant )
        };
    }

    inline bytes_t ll_channel_map_ind( std::uint64_t map, std::uint16_t instant )
    {
        return {
            0x01,                                   // LL_CHANNEL_MAP_IND
            static_cast< std::uint8_t >( map ),     // map
            static_cast< std::uint8_t >( map >> 8 ),
            static_cast< std::uint8_t >( map >> 16 ),
            static_cast< std::uint8_t >( map >> 24 ),
            static_cast< std::uint8_t >( map >> 32 ),
            lo( instant ), hi( instant )            // instant
        };
    }

    inline bytes_t ll_terminate_ind( std::uint8_t reason )
    {
        return { 0x02, reason };                    // LL_TERMINATE_IND
    }

    inline bytes_t ll_unknown_rsp( std::uint8_t opcode )
    {
        return { 0x07, opcode };                    // LL_UNKNOWN_RSP
    }

    inline bytes_t ll_feature_req( std::uint64_t features )
    {
        return {
            0x08,                                   // LL_FEATURE_REQ
            static_cast< std::uint8_t >( features ),
            static_cast< std::uint8_t >( features >> 8 ),
            static_cast< std::uint8_t >( features >> 16 ),
            static_cast< std::uint8_t >( features >> 24 ),
            static_cast< std::uint8_t >( features >> 32 ),
            static_cast< std::uint8_t >( features >> 40 ),
            static_cast< std::uint8_t >( features >> 48 ),
            static_cast< std::uint8_t >( features >> 56 )
        };
    }

    inline bytes_t ll_feature_rsp( std::uint64_t features )
    {
        bytes_t result = ll_feature_req( features );
        result[ 0 ] = 0x09;                         // LL_FEATURE_RSP

        return result;
    }

    inline bytes_t ll_version_ind( std::uint8_t version, std::uint16_t company, std::uint16_t subversion )
    {
        return {
            0x0c,                                   // LL_VERSION_IND
            version,                                // VersNr
            lo( company ), hi( company ),           // CompId
            lo( subversion ), hi( subversion )      // SubVersNr
        };
    }

    inline bytes_t ll_reject_ind( std::uint8_t reason )
    {
        return { 0x0d, reason };                    // LL_REJECT_IND
    }

    /**
     * @brief an LL_CONNECTION_PARAM_REQ with no preferred periodicity, no reference event and
     *        no offsets, the only form the tests use
     */
    inline bytes_t ll_connection_param_req(
        std::uint16_t min_interval, std::uint16_t max_interval, std::uint16_t latency, std::uint16_t timeout )
    {
        return {
            0x0f,                                   // LL_CONNECTION_PARAM_REQ
            lo( min_interval ), hi( min_interval ),
            lo( max_interval ), hi( max_interval ),
            lo( latency ), hi( latency ),
            lo( timeout ), hi( timeout ),
            0x00,                                   // preferred periodicity: none
            0x00, 0x00,                             // ReferenceConnEventCount
            0xff, 0xff,                             // Offset0 to Offset5: none
            0xff, 0xff,
            0xff, 0xff,
            0xff, 0xff,
            0xff, 0xff,
            0xff, 0xff
        };
    }

    inline bytes_t ll_reject_ext_ind( std::uint8_t opcode, std::uint8_t reason )
    {
        return { 0x11, opcode, reason };            // LL_REJECT_EXT_IND
    }

    inline bytes_t ll_ping_req()
    {
        return { 0x12 };                            // LL_PING_REQ
    }

    inline bytes_t ll_ping_rsp()
    {
        return { 0x13 };                            // LL_PING_RSP
    }

    inline bytes_t ll_phy_req( std::uint8_t transmit, std::uint8_t receive )
    {
        return { 0x16, transmit, receive };         // LL_PHY_REQ
    }

    inline bytes_t ll_phy_rsp( std::uint8_t transmit, std::uint8_t receive )
    {
        return { 0x17, transmit, receive };         // LL_PHY_RSP
    }

    inline bytes_t ll_phy_update_ind( std::uint8_t central_to_peripheral, std::uint8_t peripheral_to_central, std::uint16_t instant )
    {
        return {
            0x18,                                   // LL_PHY_UPDATE_IND
            central_to_peripheral, peripheral_to_central,
            lo( instant ), hi( instant )
        };
    }
    /** @} */

    /**
     * @brief an ATT exchange MTU request as the L2CAP payload of a data PDU
     */
    inline bytes_t att_exchange_mtu_request( std::uint16_t mtu )
    {
        return {
            0x03, 0x00,                             // L2CAP length
            0x04, 0x00,                             // L2CAP channel: ATT
            0x02, lo( mtu ), hi( mtu )              // ATT exchange MTU request
        };
    }
}

#endif
