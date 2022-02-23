#ifndef BLUETOE_BINDING_NORDIC_NRF52_SECURITY_TOOL_BOX_HPP
#define BLUETOE_BINDING_NORDIC_NRF52_SECURITY_TOOL_BOX_HPP

#include <bluetoe/security_manager.hpp>

#include <tuple>
#include <algorithm>

namespace bluetoe
{
    namespace nrf52_details
    {
        std::uint32_t random_number32();
        std::uint64_t random_number64();
        bluetoe::details::uint128_t aes_le( const bluetoe::details::uint128_t& key, const bluetoe::details::uint128_t& data );

        /**
         * @brief set of security tool box functions, both for legacy pairing and LESC pairing
         *
         * It's expected that only the required set of functions are requested by the linker.
         */
        class security_tool_box
        {
        public:
            /**
             * security tool box required by legacy pairing
             */
            bluetoe::details::uint128_t create_srand();

            bluetoe::details::longterm_key_t create_long_term_key();

            bluetoe::details::uint128_t c1(
                const bluetoe::details::uint128_t& temp_key,
                const bluetoe::details::uint128_t& rand,
                const bluetoe::details::uint128_t& p1,
                const bluetoe::details::uint128_t& p2 ) const;

            bluetoe::details::uint128_t s1(
                const bluetoe::details::uint128_t& temp_key,
                const bluetoe::details::uint128_t& srand,
                const bluetoe::details::uint128_t& mrand );

            /**
             * security tool box required by LESC pairing
             */
            bool is_valid_public_key( const std::uint8_t* public_key ) const;

            std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys();

            bluetoe::details::uint128_t select_random_nonce();

            bluetoe::details::ecdh_shared_secret_t p256( const std::uint8_t* private_key, const std::uint8_t* public_key );

            bluetoe::details::uint128_t f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z );

            std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
                const bluetoe::details::ecdh_shared_secret_t dh_key,
                const bluetoe::details::uint128_t& nonce_central,
                const bluetoe::details::uint128_t& nonce_periperal,
                const bluetoe::link_layer::device_address& addr_controller,
                const bluetoe::link_layer::device_address& addr_peripheral );

            bluetoe::details::uint128_t f6(
                const bluetoe::details::uint128_t& key,
                const bluetoe::details::uint128_t& n1,
                const bluetoe::details::uint128_t& n2,
                const bluetoe::details::uint128_t& r,
                const bluetoe::details::io_capabilities_t& io_caps,
                const bluetoe::link_layer::device_address& addr_controller,
                const bluetoe::link_layer::device_address& addr_peripheral );

            std::uint32_t g2(
                const std::uint8_t*                 u,
                const std::uint8_t*                 v,
                const bluetoe::details::uint128_t&  x,
                const bluetoe::details::uint128_t&  y );

            /**
             * Functions required by IO capabilties
             */
            bluetoe::details::uint128_t create_passkey();
        };

    }
}

#endif
