#include <bluetoe/nrf52_ccm.hpp>
#include <bluetoe/nrf52_radio.hpp>
#include <bluetoe/security_tool_box.hpp>
#include <bluetoe/bits.hpp>

#include <nrf.h>

#include <algorithm>

namespace bluetoe
{
    namespace nrf52_details
    {
        namespace
        {
            /*
             * The CCM's data structure: the key, the 39 bit packet counter, three unused
             * bytes, the direction bit and the IV.
             */
            constexpr std::size_t   key_offset          = 0;
            constexpr std::size_t   counter_offset      = 16;
            constexpr std::size_t   direction_offset    = 24;
            constexpr std::size_t   iv_offset           = 25;
            constexpr std::size_t   data_structure_size = 33;

            constexpr std::uint8_t  central_to_peripheral = 0x01;
            constexpr std::uint8_t  peripheral_to_central = 0x00;

            constexpr std::size_t   header_size         = 3;
            constexpr std::size_t   mic_size            = 4;

            // the temporary storage the CCM asks for: 16 bytes and the longest packet
            constexpr std::size_t   scratch_size        = 16 + link_layer::max_payload_size;

            struct alignas( 4 ) data_structure_t
            {
                std::uint8_t data[ data_structure_size ];
            } data_structure;

            struct alignas( 4 ) scratch_t
            {
                std::uint8_t data[ scratch_size ];
            } scratch;

            void set_counter( const packet_counter& counter, std::uint8_t direction )
            {
                bluetoe::details::write_32bit( &data_structure.data[ counter_offset ], counter.low );
                data_structure.data[ counter_offset + 4 ] = counter.high;
                data_structure.data[ direction_offset ]   = direction;
            }

            std::uint32_t mode( std::uint32_t direction, bool two_mbit )
            {
                return ( direction << CCM_MODE_MODE_Pos )
                    | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos )
                    | ( ( two_mbit ? CCM_MODE_DATARATE_2Mbit : CCM_MODE_DATARATE_1Mbit ) << CCM_MODE_DATARATE_Pos );
            }

        }

        void ccm::begin_event( const encryption_t& encryption )
        {
            std::copy( std::begin( encryption.key ), std::end( encryption.key ), &data_structure.data[ key_offset ] );
            std::copy( std::begin( encryption.iv ), std::end( encryption.iv ), &data_structure.data[ iv_offset ] );

            NRF_CCM->CNFPTR     = reinterpret_cast< std::uint32_t >( &data_structure );
            NRF_CCM->SCRATCHPTR = reinterpret_cast< std::uint32_t >( &scratch );
            NRF_CCM->INTENCLR   = 0xffffffff;
        }

        link_layer::read_buffer ccm::prepare_reception(
            const encryption_t& encryption, link_layer::read_buffer room, link_layer::read_buffer ciphertext, bool two_mbit )
        {
            set_counter( encryption.receive_counter, central_to_peripheral );

            /*
             * Disabled and enabled again before every reception: the CCM was seen to start
             * decrypting before the packet had arrived, and to write past the room when the
             * stale length in the ciphertext was long enough.
             * (https://devzone.nordicsemi.com/f/nordic-q-a/43656/what-causes-decryption-before-receiving)
             */
            NRF_CCM->ENABLE         = CCM_ENABLE_ENABLE_Disabled << CCM_ENABLE_ENABLE_Pos;
            NRF_CCM->ENABLE         = CCM_ENABLE_ENABLE_Enabled << CCM_ENABLE_ENABLE_Pos;
            NRF_CCM->MODE           = mode( CCM_MODE_MODE_Decryption, two_mbit );
            NRF_CCM->INPTR          = reinterpret_cast< std::uint32_t >( ciphertext.buffer );
            NRF_CCM->OUTPTR         = reinterpret_cast< std::uint32_t >( room.buffer );
            NRF_CCM->MAXPACKETSIZE  = room.size - header_size;
            NRF_CCM->SHORTS         = 0;

            NRF_CCM->EVENTS_ENDKSGEN = 0;
            NRF_CCM->EVENTS_ENDCRYPT = 0;
            NRF_CCM->EVENTS_ERROR    = 0;

            NRF_CCM->TASKS_KSGEN = 1;
            NRF_PPI->CHENSET     = ppi_address_ccm_crypt;

            return { ciphertext.buffer, room.size + mic_size };
        }

        /*
         * The decryption runs at the data rate and ends a few microseconds after the
         * packet, with the MIC. An empty PDU is not decrypted, so there is nothing to
         * wait for.
         */
        bool ccm::decryption_pending( std::uint32_t air_payload_size )
        {
            NRF_PPI->CHENCLR = ppi_address_ccm_crypt;

            if ( air_payload_size == 0 )
                return false;

            // the interrupt is armed before the event is looked at, so that an event that
            // sets in between is caught either way
            NRF_CCM->INTENSET = CCM_INTENSET_ENDCRYPT_Msk;

            if ( !NRF_CCM->EVENTS_ENDCRYPT )
                return true;

            NRF_CCM->INTENCLR = CCM_INTENCLR_ENDCRYPT_Msk;
            NVIC_ClearPendingIRQ( CCM_AAR_IRQn );

            return false;
        }

        bool ccm::reception_authentic( link_layer::read_buffer room, const std::uint8_t* ciphertext, std::uint32_t air_payload_size )
        {
            // an empty PDU carries no MIC and is not decrypted; the CCM leaves the room alone
            if ( air_payload_size == 0 )
            {
                std::copy( ciphertext, ciphertext + header_size, room.buffer );

                return true;
            }

            if ( !NRF_CCM->EVENTS_ENDCRYPT || NRF_CCM->EVENTS_ERROR )
                return false;

            return ( NRF_CCM->MICSTATUS & CCM_MICSTATUS_MICSTATUS_Msk ) == ( CCM_MICSTATUS_MICSTATUS_CheckPassed << CCM_MICSTATUS_MICSTATUS_Pos );
        }

        link_layer::write_buffer ccm::prepare_transmission(
            const encryption_t& encryption, link_layer::write_buffer pdu, link_layer::read_buffer ciphertext, bool two_mbit )
        {
            // an empty PDU goes out as it is
            if ( pdu.buffer[ 1 ] == 0 )
                return pdu;

            NRF_PPI->CHENCLR = ppi_address_ccm_crypt;

            set_counter( encryption.transmit_counter, peripheral_to_central );

            // the key stream first, then the encryption, ahead of the transmitter at its rate
            NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Enabled << CCM_ENABLE_ENABLE_Pos;
            NRF_CCM->MODE   = mode( CCM_MODE_MODE_Encryption, two_mbit );
            NRF_CCM->INPTR  = reinterpret_cast< std::uint32_t >( pdu.buffer );
            NRF_CCM->OUTPTR = reinterpret_cast< std::uint32_t >( ciphertext.buffer );
            NRF_CCM->SHORTS = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;

            NRF_CCM->EVENTS_ENDKSGEN = 0;
            NRF_CCM->EVENTS_ENDCRYPT = 0;
            NRF_CCM->EVENTS_ERROR    = 0;

            NRF_CCM->TASKS_KSGEN = 1;

            return { ciphertext.buffer, pdu.size + mic_size };
        }

        void ccm::advance( encryption_t& encryption, bool received_encrypted_pdu, bool acknowledged_encrypted_pdu )
        {
            if ( received_encrypted_pdu )
                encryption.receive_counter.increment();

            if ( acknowledged_encrypted_pdu )
                encryption.transmit_counter.increment();
        }

        /*
         * The session key is the long term key applied to the diversifier, central's half
         * first; the CCM reads the key most significant byte first, which is the reverse of
         * how the security manager's functions return it.
         */
        std::pair< std::uint64_t, std::uint32_t > ccm::setup_encryption(
            encryption_t& encryption, const bluetoe::details::uint128_t& key, std::uint64_t skdm, std::uint32_t ivm )
        {
            const std::uint64_t skds = random_number64();
            const std::uint32_t ivs  = random_number32();

            bluetoe::details::uint128_t diversifier;
            bluetoe::details::write_64bit( &diversifier[ 0 ], skdm );
            bluetoe::details::write_64bit( &diversifier[ 8 ], skds );

            const bluetoe::details::uint128_t session_key = aes_le( key, diversifier );

            std::copy( session_key.rbegin(), session_key.rend(), encryption.key );
            bluetoe::details::write_64bit( encryption.iv, static_cast< std::uint64_t >( ivm ) | ( static_cast< std::uint64_t >( ivs ) << 32 ) );

            encryption.receive_counter  = packet_counter();
            encryption.transmit_counter = packet_counter();

            return { skds, ivs };
        }

        void ccm::enable_interrupt()
        {
            NRF_CCM->INTENCLR = 0xffffffff;
            NVIC_SetPriority( CCM_AAR_IRQn, 0 );
            NVIC_ClearPendingIRQ( CCM_AAR_IRQn );
            NVIC_EnableIRQ( CCM_AAR_IRQn );
        }
    }
}

extern "C" void CCM_AAR_IRQHandler()
{
    NRF_CCM->INTENCLR = CCM_INTENCLR_ENDCRYPT_Msk;

    if ( bluetoe::nrf52_details::interrupts.ccm )
        bluetoe::nrf52_details::interrupts.ccm();
}
