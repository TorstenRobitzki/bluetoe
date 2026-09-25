#include "ll_pdus.hpp"

namespace test {

    namespace {

        std::uint8_t lo( std::uint32_t value )
        {
            return static_cast< std::uint8_t >( value );
        }

        std::uint8_t hi( std::uint32_t value )
        {
            return static_cast< std::uint8_t >( value >> 8 );
        }

        // a number on air, least significant octet first
        void append( bytes_t& pdu, std::uint64_t value, std::size_t octets )
        {
            for ( std::size_t i = 0; i != octets; ++i )
                pdu.push_back( static_cast< std::uint8_t >( value >> ( 8 * i ) ) );
        }

        // an LL_CONNECTION_PARAM_REQ or an LL_CONNECTION_PARAM_RSP, which share their fields
        bytes_t ll_connection_param_pdu( std::uint8_t opcode, const connection_parameter_request& p )
        {
            return {
                opcode,
                lo( p.min_interval ), hi( p.min_interval ),
                lo( p.max_interval ), hi( p.max_interval ),
                lo( p.latency ), hi( p.latency ),
                lo( p.timeout ), hi( p.timeout ),
                p.preferred_periodicity,
                lo( p.reference_event ), hi( p.reference_event ),   // ReferenceConnEventCount
                lo( p.offsets[ 0 ] ), hi( p.offsets[ 0 ] ),         // Offset0 to Offset5
                lo( p.offsets[ 1 ] ), hi( p.offsets[ 1 ] ),
                lo( p.offsets[ 2 ] ), hi( p.offsets[ 2 ] ),
                lo( p.offsets[ 3 ] ), hi( p.offsets[ 3 ] ),
                lo( p.offsets[ 4 ] ), hi( p.offsets[ 4 ] ),
                lo( p.offsets[ 5 ] ), hi( p.offsets[ 5 ] )
            };
        }
    }

    bytes_t ll_pdu( std::uint8_t llid, const bytes_t& payload )
    {
        bytes_t result = { llid, static_cast< std::uint8_t >( payload.size() ) };
        result.insert( result.end(), payload.begin(), payload.end() );

        return result;
    }

    bytes_t ll_control( const bytes_t& payload )
    {
        return ll_pdu( llid::control, payload );
    }

    bytes_t ll_empty()
    {
        return ll_pdu( llid::continuation, {} );
    }

    bytes_t connect_ind( const connection_parameters& p )
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

    bytes_t ll_connection_update_ind( const connection_update& u )
    {
        return {
            0x00,                                   // LL_CONNECTION_UPDATE_IND
            u.window_size,
            lo( u.window_offset ), hi( u.window_offset ),
            lo( u.interval ), hi( u.interval ),
            lo( u.latency ), hi( u.latency ),
            lo( u.timeout ), hi( u.timeout ),
            lo( u.instant ), hi( u.instant )
        };
    }

    bytes_t ll_channel_map_ind( std::uint64_t map, std::uint16_t instant )
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

    bytes_t ll_terminate_ind( std::uint8_t reason )
    {
        return { 0x02, reason };                    // LL_TERMINATE_IND
    }

    bytes_t ll_enc_req( std::uint64_t rand, std::uint16_t ediv, std::uint64_t skdm, std::uint32_t ivm )
    {
        bytes_t result = { 0x03 };                  // LL_ENC_REQ
        append( result, rand, 8 );                  // Rand
        append( result, ediv, 2 );                  // EDIV
        append( result, skdm, 8 );                  // SKDm
        append( result, ivm, 4 );                   // IVm

        return result;
    }

    bytes_t ll_enc_rsp( std::uint64_t skds, std::uint32_t ivs )
    {
        bytes_t result = { 0x04 };                  // LL_ENC_RSP
        append( result, skds, 8 );                  // SKDs
        append( result, ivs, 4 );                   // IVs

        return result;
    }

    bytes_t ll_start_enc_req()
    {
        return { 0x05 };                            // LL_START_ENC_REQ
    }

    bytes_t ll_start_enc_rsp()
    {
        return { 0x06 };                            // LL_START_ENC_RSP
    }

    bytes_t ll_pause_enc_req()
    {
        return { 0x0a };                            // LL_PAUSE_ENC_REQ
    }

    bytes_t ll_pause_enc_rsp()
    {
        return { 0x0b };                            // LL_PAUSE_ENC_RSP
    }

    bytes_t ll_unknown_rsp( std::uint8_t opcode )
    {
        return { 0x07, opcode };                    // LL_UNKNOWN_RSP
    }

    bytes_t ll_feature_req( std::uint64_t features )
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

    bytes_t ll_feature_rsp( std::uint64_t features )
    {
        bytes_t result = ll_feature_req( features );
        result[ 0 ] = 0x09;                         // LL_FEATURE_RSP

        return result;
    }

    bytes_t ll_version_ind( std::uint8_t version, std::uint16_t company, std::uint16_t subversion )
    {
        return {
            0x0c,                                   // LL_VERSION_IND
            version,                                // VersNr
            lo( company ), hi( company ),           // CompId
            lo( subversion ), hi( subversion )      // SubVersNr
        };
    }

    bytes_t ll_reject_ind( std::uint8_t reason )
    {
        return { 0x0d, reason };                    // LL_REJECT_IND
    }

    bytes_t ll_connection_param_req( const connection_parameter_request& p )
    {
        return ll_connection_param_pdu( 0x0f, p );  // LL_CONNECTION_PARAM_REQ
    }

    bytes_t ll_connection_param_rsp( const connection_parameter_request& p )
    {
        return ll_connection_param_pdu( 0x10, p );  // LL_CONNECTION_PARAM_RSP
    }

    bytes_t ll_reject_ext_ind( std::uint8_t opcode, std::uint8_t reason )
    {
        return { 0x11, opcode, reason };            // LL_REJECT_EXT_IND
    }

    bytes_t ll_ping_req()
    {
        return { 0x12 };                            // LL_PING_REQ
    }

    bytes_t ll_ping_rsp()
    {
        return { 0x13 };                            // LL_PING_RSP
    }

    bytes_t ll_phy_req( std::uint8_t transmit, std::uint8_t receive )
    {
        return { 0x16, transmit, receive };         // LL_PHY_REQ
    }

    bytes_t ll_phy_rsp( std::uint8_t transmit, std::uint8_t receive )
    {
        return { 0x17, transmit, receive };         // LL_PHY_RSP
    }

    bytes_t ll_phy_update_ind( std::uint8_t central_to_peripheral, std::uint8_t peripheral_to_central, std::uint16_t instant )
    {
        return {
            0x18,                                   // LL_PHY_UPDATE_IND
            central_to_peripheral, peripheral_to_central,
            lo( instant ), hi( instant )
        };
    }

    pattern_t l2cap_connection_parameter_update_request(
        std::uint16_t min_interval, std::uint16_t max_interval, std::uint16_t latency, std::uint16_t timeout )
    {
        return {
            X, X,                                   // L2CAP length
            0x05, 0x00,                             // L2CAP channel: LE signaling
            0x12,                                   // connection parameter update request
            X,                                      // identifier
            0x08, 0x00,                             // length
            lo( min_interval ), hi( min_interval ),
            lo( max_interval ), hi( max_interval ),
            lo( latency ), hi( latency ),
            lo( timeout ), hi( timeout )
        };
    }

    bytes_t att_exchange_mtu_request( std::uint16_t mtu )
    {
        return {
            0x03, 0x00,                             // L2CAP length
            0x04, 0x00,                             // L2CAP channel: ATT
            0x02, lo( mtu ), hi( mtu )              // ATT exchange MTU request
        };
    }
}
