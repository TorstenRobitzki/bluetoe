#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_TEST_TOOLS_ENCRYPTION_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_TEST_TOOLS_ENCRYPTION_HPP

/**
 * @file encryption.hpp
 *
 * The encryption of a connection as a test computes it on the host: the session key from the
 * long term key and the two halves of the diversifier, the way the device derives it, and the
 * CCM of the Core Specification, Vol 6 Part E, over data channel PDUs. The tester carries
 * ciphertext as bytes it knows nothing about; this is what makes them and checks them.
 */

#include "host/central.hpp"

#include <bluetoe/security_connection_data.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief who sent a PDU, the direction bit of the nonce
     */
    enum class encryption_direction
    {
        central_to_peripheral,
        peripheral_to_central
    };

    /**
     * @brief the encryption of one connection
     *
     * The long term key is the security manager's, least significant byte first; the four
     * halves are the numbers LL_ENC_REQ and LL_ENC_RSP carry.
     */
    class encryption_session
    {
    public:
        encryption_session(
            const bluetoe::details::uint128_t& long_term_key,
            std::uint64_t skdm, std::uint64_t skds, std::uint32_t ivm, std::uint32_t ivs );

        /**
         * @brief the ciphertext of `payload` with its MIC appended
         *
         * `packet_counter` is the count of encrypted PDUs sent in `direction` before this one;
         * `kind` is the PDU's LLID, the only part of the header the MIC covers.
         */
        std::vector< std::uint8_t > encrypt(
            encryption_direction direction, std::uint64_t packet_counter, llid kind, std::span< const std::uint8_t > payload ) const;

        /**
         * @brief the plaintext of `ciphertext`, a payload with its MIC, or nothing if the MIC
         *        does not check
         */
        std::optional< std::vector< std::uint8_t > > decrypt(
            encryption_direction direction, std::uint64_t packet_counter, llid kind, std::span< const std::uint8_t > ciphertext ) const;

        /**
         * @brief the session key, most significant byte first
         */
        const std::array< std::uint8_t, 16 >& key() const;

    private:
        std::array< std::uint8_t, 13 > nonce( encryption_direction direction, std::uint64_t packet_counter ) const;
        std::array< std::uint8_t, 16 > encrypt_block( const std::array< std::uint8_t, 16 >& block ) const;
        std::array< std::uint8_t, 4 > mic(
            const std::array< std::uint8_t, 13 >& nonce, llid kind, std::span< const std::uint8_t > plaintext ) const;
        std::vector< std::uint8_t > key_stream( const std::array< std::uint8_t, 13 >& nonce, std::size_t size ) const;

        std::array< std::uint8_t, 16 > key_;
        std::array< std::uint8_t, 8 >  iv_;
    };
}
}

#endif
