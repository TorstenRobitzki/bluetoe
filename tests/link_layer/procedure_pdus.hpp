#ifndef BLUETOE_TESTS_LINK_LAYER_PROCEDURE_PDUS_HPP
#define BLUETOE_TESTS_LINK_LAYER_PROCEDURE_PDUS_HPP

#include "procedure_mocks.hpp"

#include <cstdint>
#include <vector>

/*
 * LL control PDUs that the tests of several procedures send, in the layout of the mocks.
 */
inline const auto ping_req = control_pdu( {
    0x12                // LL_PING_REQ
} );

/*
 * A central that supports every feature: the answer is the same whether the peripheral
 * reports its own features or the features both support.
 */
inline const auto feature_req = control_pdu( {
    0x08,                                           // LL_FEATURE_REQ
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // FeatureSet: all features
} );

inline const auto version_ind = control_pdu( {
    0x0C,               // LL_VERSION_IND
    0x0D,               // VersNr: Core 5.4
    0x59, 0x00,         // CompId: Nordic Semiconductor
    0x34, 0x12          // SubVersNr
} );

// the values of link_layer_mock
inline const auto own_version_ind = control_pdu( {
    0x0C,               // LL_VERSION_IND
    0x09,               // VersNr: Core 5.0
    0x69, 0x02,         // CompId: 0x0269
    0x00, 0x00          // SubVersNr
} );

inline const auto terminate_ind = control_pdu( {
    0x02,               // LL_TERMINATE_IND
    0x13                // ErrorCode: Remote User Terminated Connection
} );

// the fields of an LL_CONNECTION_UPDATE_IND; by default those of the first connection update test
struct connection_update_fields
{
    std::uint8_t    window_size     = 2;
    std::uint16_t   window_offset   = 5;
    std::uint16_t   interval        = 40;
    std::uint16_t   latency         = 3;
    std::uint16_t   timeout         = 300;
    std::uint16_t   instant         = 106;
};

inline std::vector< std::uint8_t > connection_update_ind( const connection_update_fields& fields )
{
    const auto lsb = []( std::uint16_t value ){ return static_cast< std::uint8_t >( value ); };
    const auto msb = []( std::uint16_t value ){ return static_cast< std::uint8_t >( value >> 8 ); };

    return control_pdu( {
        0x00, fields.window_size,
        lsb( fields.window_offset ), msb( fields.window_offset ),
        lsb( fields.interval ), msb( fields.interval ),
        lsb( fields.latency ), msb( fields.latency ),
        lsb( fields.timeout ), msb( fields.timeout ),
        lsb( fields.instant ), msb( fields.instant ) } );
}

// the connection update of the first connection update test, with another instant
inline std::vector< std::uint8_t > connection_update_ind( std::uint16_t instant )
{
    return connection_update_ind( connection_update_fields{ .instant = instant } );
}

// the channel map update of the first channel map update test, with another instant
inline std::vector< std::uint8_t > channel_map_ind( std::uint16_t instant )
{
    return control_pdu( {
        0x01, 0xFF, 0x00, 0xFF, 0x00, 0x1F,
        static_cast< std::uint8_t >( instant ), static_cast< std::uint8_t >( instant >> 8 ) } );
}

#endif
