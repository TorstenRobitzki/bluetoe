#ifndef BLUETOE_TESTS_TEST_TOOLS_LL_PDUS_HPP
#define BLUETOE_TESTS_TEST_TOOLS_LL_PDUS_HPP

/**
 * @file ll_pdus.hpp
 *
 * The PDUs the link layer tests send and expect, built by name with their fields as
 * parameters. The first test of every kind of PDU constructs it by hand, bytes with the
 * field names, to be read against the specification; the tests after it build the same
 * PDU here, and the link layer, proven on the bytes, would answer a wrong builder
 * differently. So nothing tests the builders, and a test spells out only the PDU it is
 * about. Every builder returns the bytes as they are on air; the ones of link layer
 * control PDUs return the payload behind the data channel header, which is what a test's
 * ll_control_pdu() takes, and ll_control() and ll_empty() put the header in front where a
 * whole PDU is needed.
 */

#include "test_radio.hpp"

#include <array>
#include <cstdint>
#include <type_traits>
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

    /**
     * @brief a data channel PDU with `llid`, SN and NESN clear, around `payload`
     */
    bytes_t ll_pdu( std::uint8_t llid, const bytes_t& payload );

    bytes_t ll_control( const bytes_t& payload );

    bytes_t ll_empty();

    /**
     * @brief the parameters of a CONNECT_IND, the defaults being the connection the tests
     *        run on: a 30 ms interval, no latency, a 720 ms timeout, all channels
     */
    struct connection_parameters
    {
        std::uint8_t    window_size             = 0x03;
        std::uint16_t   window_offset           = 0x000b;
        std::uint16_t   interval                = 0x0018;
        std::uint16_t   latency                 = 0x0000;
        std::uint16_t   timeout                 = 0x0048;
        std::uint64_t   channel_map             = 0x1fffffffff;
        std::uint8_t    hop                     = 10;
        std::uint8_t    sleep_clock_accuracy    = 5;
    };

    /**
     * @brief a CONNECT_IND from the initiator 48:f0:92:62:1c:3c to the advertiser
     *        c0:0f:15:08:11:47, both random, on access address 0xaf9ab35a
     */
    bytes_t connect_ind( const connection_parameters& p = {} );

    /**
     * @brief `value` with one of its fields changed, for a PDU that differs from a named one
     *        in one field
     *
     * @code
     * with( valid_update, &connection_update::window_size, 205 )
     * @endcode
     */
    template < typename T, typename Field >
    T with( T value, Field T::* field, std::type_identity_t< Field > new_value )
    {
        value.*field = new_value;

        return value;
    }

    /**
     * @brief the fields of an LL_CONNECTION_UPDATE_IND
     */
    struct connection_update
    {
        std::uint8_t    window_size;
        std::uint16_t   window_offset;
        std::uint16_t   interval;
        std::uint16_t   latency;
        std::uint16_t   timeout;
        std::uint16_t   instant;
    };

    /**
     * @name link layer control PDUs, the payload behind the data channel header
     * @{
     */
    bytes_t ll_connection_update_ind( const connection_update& update );

    bytes_t ll_channel_map_ind( std::uint64_t map, std::uint16_t instant );

    bytes_t ll_terminate_ind( std::uint8_t reason );

    /**
     * @brief an LL_ENC_REQ; the numbers as the specification prints them, most significant
     *        octet first, go on air least significant octet first
     */
    bytes_t ll_enc_req( std::uint64_t rand, std::uint16_t ediv, std::uint64_t skdm, std::uint32_t ivm );

    bytes_t ll_enc_rsp( std::uint64_t skds, std::uint32_t ivs );

    bytes_t ll_start_enc_req();

    bytes_t ll_start_enc_rsp();

    bytes_t ll_pause_enc_req();

    bytes_t ll_pause_enc_rsp();

    bytes_t ll_unknown_rsp( std::uint8_t opcode );

    bytes_t ll_feature_req( std::uint64_t features );

    bytes_t ll_feature_rsp( std::uint64_t features );

    bytes_t ll_version_ind( std::uint8_t version, std::uint16_t company, std::uint16_t subversion );

    bytes_t ll_reject_ind( std::uint8_t reason );

    /**
     * @brief the fields of an LL_CONNECTION_PARAM_REQ and of an LL_CONNECTION_PARAM_RSP
     *
     * The defaults are no preferred periodicity, a reference event of zero and no offsets.
     */
    struct connection_parameter_request
    {
        std::uint16_t                   min_interval;
        std::uint16_t                   max_interval;
        std::uint16_t                   latency;
        std::uint16_t                   timeout;
        std::uint8_t                    preferred_periodicity   = 0;
        std::uint16_t                   reference_event         = 0;
        std::array< std::uint16_t, 6 >  offsets                 = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
    };

    bytes_t ll_connection_param_req( const connection_parameter_request& p );

    bytes_t ll_connection_param_rsp( const connection_parameter_request& p );

    bytes_t ll_reject_ext_ind( std::uint8_t opcode, std::uint8_t reason );

    bytes_t ll_ping_req();

    bytes_t ll_ping_rsp();

    /**
     * @brief the PHYs an LL_PHY_REQ, LL_PHY_RSP or LL_PHY_UPDATE_IND names, as bits
     */
    namespace phy {
        constexpr std::uint8_t none     = 0x00;
        constexpr std::uint8_t le_1m    = 0x01;
        constexpr std::uint8_t le_2m    = 0x02;
        constexpr std::uint8_t le_coded = 0x04;
    }

    bytes_t ll_phy_req( std::uint8_t transmit, std::uint8_t receive );

    bytes_t ll_phy_rsp( std::uint8_t transmit, std::uint8_t receive );

    bytes_t ll_phy_update_ind( std::uint8_t central_to_peripheral, std::uint8_t peripheral_to_central, std::uint16_t instant );
    /** @} */

    /**
     * @brief the pattern of an L2CAP connection parameter update request the link layer sends,
     *        whatever identifier it chose, as check_outgoing_l2cap_pdu() sees it: behind the
     *        data channel header
     */
    pattern_t l2cap_connection_parameter_update_request(
        std::uint16_t min_interval, std::uint16_t max_interval, std::uint16_t latency, std::uint16_t timeout );

    /**
     * @brief an ATT exchange MTU request as the L2CAP payload of a data PDU
     */
    bytes_t att_exchange_mtu_request( std::uint16_t mtu );
}

#endif
