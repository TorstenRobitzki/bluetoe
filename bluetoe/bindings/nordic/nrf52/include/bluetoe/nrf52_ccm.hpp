#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_CCM_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_CCM_HPP

/**
 * @file nrf52_ccm.hpp
 *
 * The encryption of connection events with the CCM peripheral, for the radio of
 * nrf52_radio.hpp. The radio's encrypting variant calls it at the points of a connection
 * event where encryption matters; the other variant has those calls compiled out, so
 * that it neither links this code nor pays for its RAM.
 *
 * @section layout What the CCM reads and writes
 *
 * The CCM works on packets of three header bytes, S0, the length and one spare byte, and
 * the payload, which is why the radio stores every PDU that way (encrypted_pdu_layout) and
 * has the RADIO include the spare byte in memory without sending it. Decrypting, it
 * reads the ciphertext the RADIO received into the radio's scratch area, as the packet
 * arrives, and writes the plaintext into the room of the link layer's buffer; the length
 * it writes is the one on air less the MIC. Encrypting, it reads the PDU from the buffer
 * and writes the ciphertext, with the MIC, into the scratch area the RADIO then sends
 * from, staying ahead of the transmitter at the data rate of the PHY. The scratch area is
 * free for that: a reception has ended and been decrypted before the answer is prepared.
 *
 * The CCM decrypts as many bytes as the ciphertext's length byte says, before the CRC has
 * judged the packet, so it has to be kept from writing past the room. The nRF52820, nRF52833
 * and nRF52840 bound it with MAXPACKETSIZE; the nRF52832 has no such register, and there the
 * CCM decrypts into a plaintext of the largest size that is copied into the room once the
 * packet is judged authentic, at the price of that much RAM on that part.
 *
 * @section counters The packet counters
 *
 * Each direction has a 39 bit packet counter that is part of the nonce. The counter of a
 * direction advances with every encrypted PDU of that direction that is new, not with a
 * repeated one; the link layer's buffer is the one that knows which is which, and reports
 * it with every reception (link_layer::reception_result), and the radio advances the
 * counters in the connection's encryption_t. Empty PDUs go unencrypted, in both
 * directions, and use no counter value; neither do the PDUs from before a switch, which
 * is why the radio keeps in mind whether the PDU it last sent was a ciphertext.
 */

#include <bluetoe/address.hpp>
#include <bluetoe/buffer.hpp>
#include <bluetoe/security_connection_data.hpp>

#include <cstdint>
#include <utility>

namespace bluetoe
{
    namespace nrf52_details
    {
        struct encryption_t;

        /**
         * @brief the CCM peripheral, as the radio's encrypting variant uses it
         */
        class ccm
        {
        public:
            /**
             * @brief derives the session key and chooses the peripheral's halves of the
             *        session key diversifier and the IV; see scheduled_radio_encryption
             */
            static std::pair< std::uint64_t, std::uint32_t > setup_encryption(
                encryption_t& encryption, const bluetoe::details::uint128_t& key, std::uint64_t skdm, std::uint32_t ivm );

            /**
             * @brief the CCM's interrupt, at the radio's priority, so that neither interrupts
             *        the other; it judges a reception the radio's end interrupt found still
             *        being decrypted
             */
            static void enable_interrupt();

            /**
             * @brief the start of a connection event: the connection's key and IV go into
             *        the CCM
             */
            static void begin_event( const encryption_t& encryption );

            /**
             * @brief a reception into `room`, decrypted as it arrives: the ciphertext lands
             *        in `ciphertext`, which is returned as the buffer the RADIO receives into,
             *        with room for the MIC
             */
            static link_layer::read_buffer prepare_reception(
                const encryption_t& encryption, link_layer::read_buffer room, link_layer::read_buffer ciphertext, bool two_mbit );

            /**
             * @brief after a reception with a valid CRC: whether the CCM is still busy with
             *        the last block and the MIC
             *
             * True, and its interrupt calls the radio's ccm_interrupt(), which judges the
             * reception then; the radio does not wait.
             */
            static bool decryption_pending( std::uint32_t air_payload_size );

            /**
             * @brief after a reception with a valid CRC of `air_payload_size` bytes on air,
             *        decrypted: whether the plaintext in the room is authentic
             *
             * An empty PDU is not encrypted and has its header copied to the room.
             */
            static bool reception_authentic(
                link_layer::read_buffer room, const std::uint8_t* ciphertext, std::uint32_t air_payload_size );

            /**
             * @brief the PDU the RADIO transmits for `pdu`: its ciphertext in `ciphertext`,
             *        with the MIC, encrypted ahead of the transmitter
             *
             * An empty PDU is returned as it is.
             */
            static link_layer::write_buffer prepare_transmission(
                const encryption_t& encryption, link_layer::write_buffer pdu, link_layer::read_buffer ciphertext, bool two_mbit );

            /**
             * @brief the counters advance: the receive counter with a new PDU that was
             *        decrypted, the transmit counter with an acknowledged PDU that went out
             *        encrypted
             */
            static void advance( encryption_t& encryption, bool received_encrypted_pdu, bool acknowledged_encrypted_pdu );
        };
    }
}

#endif
