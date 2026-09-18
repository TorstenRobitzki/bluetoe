#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_CENTRAL_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_CENTRAL_HPP

/**
 * @file central.hpp
 *
 * The central side of a connection, as a test writes it for the tester: the data channel PDUs
 * the tester transmits, with the SN, NESN and MD bits of the flow the test expects, and the checks
 * of the device's replies against them. The tester's program is loaded before the run, so the bits
 * are what the flow should be, not a reaction to what the device sent.
 */

#include "link/tester_program.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the LLID of a data channel PDU
     */
    enum class llid : std::uint8_t
    {
        continuation    = 0x01,
        start           = 0x02,
        control         = 0x03
    };

    /**
     * @brief the MD bit of a data channel PDU, named where a PDU is built, so that a bare `true`
     *        can not stand for it
     */
    enum more_data_flag : bool
    {
        no_more_data = false,
        more_data    = true
    };

    /**
     * @brief the sequence numbers of the central in one connection, both starting at zero
     *
     * send() is the normal flow: the central's previous PDU was acknowledged and the device's
     * reply to it was new. resend() and send_nack() state a deviation from it.
     */
    class central
    {
    public:
        /**
         * @brief the next PDU: the previous one acknowledged, the device's reply to it new
         */
        std::vector< std::uint8_t > send( std::span< const std::uint8_t > payload = {}, more_data_flag md = no_more_data, llid kind = llid::continuation );

        /**
         * @brief the previous PDU again, as it was not acknowledged; the device's reply to it new
         */
        std::vector< std::uint8_t > resend();

        /**
         * @brief the next PDU, the previous one acknowledged, but the device's reply to it not
         *        accepted, for example as it had an invalid CRC
         */
        std::vector< std::uint8_t > send_nack( std::span< const std::uint8_t > payload = {}, more_data_flag md = no_more_data, llid kind = llid::continuation );

    private:
        std::vector< std::uint8_t > build( std::span< const std::uint8_t > payload, more_data_flag md, llid kind );

        bool                        sent_   = false;
        bool                        sn_     = false;
        bool                        nesn_   = false;
        std::vector< std::uint8_t > last_;
    };

    /**
     * @brief the header bits of a data channel PDU
     * @{
     */
    bool sequence_number( std::span< const std::uint8_t > pdu );
    bool next_expected_sequence_number( std::span< const std::uint8_t > pdu );
    bool has_more_data( std::span< const std::uint8_t > pdu );
    /** @} */

    /**
     * @brief a data channel PDU as the link layer hands it to the PDU buffer: the LLID, the
     *        length and the payload; SN, NESN and MD are the buffer's to set
     */
    std::vector< std::uint8_t > data_pdu( llid kind, std::span< const std::uint8_t > payload );

    /**
     * @brief the device's reply to the central's `sent` in the normal flow: it acknowledges
     *        `sent` and is new to the central
     */
    std::vector< std::uint8_t > reply_to(
        std::span< const std::uint8_t > sent, std::span< const std::uint8_t > payload = {},
        more_data_flag md = no_more_data, llid kind = llid::continuation );

    /**
     * @brief whether the device's `reply` acknowledges the central's `sent`: its NESN is the SN
     *        after the one `sent` carried
     */
    bool acknowledges( const captured_pdu& reply, std::span< const std::uint8_t > sent );

    /**
     * @brief whether the device's `reply` is new to the central that `sent` it: its SN is the one
     *        `sent` expected next
     */
    bool is_new( const captured_pdu& reply, std::span< const std::uint8_t > sent );
}
}

#endif
