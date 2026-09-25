#ifndef BLUETOE_TESTS_TEST_TOOLS_SECURITY_MANAGER_MOCK_HPP
#define BLUETOE_TESTS_TEST_TOOLS_SECURITY_MANAGER_MOCK_HPP

/**
 * @file security_manager_mock.hpp
 *
 * A security manager for a link layer under test that fakes the pairing state and the set of
 * available keys: find_key() answers with key_vault and records the EDIV and Rand it was asked
 * for, and the L2CAP side does nothing.
 */

#include <bluetoe/link_layer.hpp>
#include <bluetoe/pairing_status.hpp>
#include <bluetoe/server.hpp>

#include <cstdint>
#include <utility>

namespace test {

    /**
     * @brief a service whose characteristic requires an encrypted connection
     */
    inline std::uint16_t secret_value;

    using secret_service = bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< decltype( secret_value ), &secret_value >,
                bluetoe::no_write_access
            >,
            bluetoe::requires_encryption
        >
    >;

    /**
     * @brief what find_key() answers: whether there is a key, and the key
     */
    inline std::pair< bool, bluetoe::details::uint128_t > key_vault;

    inline const bluetoe::details::uint128_t example_key = { {
        0x01, 0x80, 0x02, 0x70,
        0x03, 0x60, 0x04, 0x50,
        0x05, 0x40, 0x06, 0x30,
        0x07, 0x20, 0x08, 0x10
    } };

    /**
     * @brief the EDIV and Rand find_key() was asked for last
     */
    struct key_request
    {
        std::uint16_t ediv = 0;
        std::uint64_t rand = 0;
    };

    inline key_request last_key_request;

    /**
     * @brief the mocked security manager, an option of the link layer under test
     */
    struct security_manager
    {
        template < typename ... >
        class impl
        {
        public:
            template < class OtherConnectionData >
            class channel_data_t : public OtherConnectionData
            {
            public:
                std::pair< bool, bluetoe::details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
                {
                    last_key_request = { ediv, rand };

                    return key_vault;
                }

                void remote_connection_created( const bluetoe::link_layer::device_address& )
                {
                }

                bluetoe::device_pairing_status local_device_pairing_status() const
                {
                    return bluetoe::device_pairing_status::no_key;
                }

                template < typename Connection >
                void restore_bonded_cccds( Connection& )
                {
                }
            };

            template < class Connection >
            void l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t*, std::size_t&, Connection& )
            {
            }

            template < class Connection >
            bool security_manager_output_available( Connection& ) const
            {
                return false;
            }

            template < class Connection >
            void l2cap_output( std::uint8_t*, std::size_t&, Connection& )
            {
            }

            static constexpr std::uint16_t channel_id               = bluetoe::l2cap_channel_ids::sm;
            static constexpr std::size_t   minimum_channel_mtu_size = bluetoe::details::default_att_mtu_size;
            static constexpr std::size_t   maximum_channel_mtu_size = bluetoe::details::default_att_mtu_size;
        };

        struct meta_type :
            bluetoe::details::security_manager_meta_type,
            bluetoe::link_layer::details::valid_link_layer_option_meta_type {};
    };
}

#endif
