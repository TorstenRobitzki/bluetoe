#ifndef BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP
#define BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP

#include "aes.h"

namespace test {

    struct security_functions {
        bluetoe::link_layer::device_address local_addr() const
        {
            return bluetoe::link_layer::device_address();
        }

        bluetoe::details::uint128_t create_srand() const
        {
            return { { 0 } };
        }

        bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const bluetoe::details::uint128_t& b ) const
        {
            std::transform(
                a.begin(), a.end(),
                b.begin(),
                a.begin(),
                []( std::uint8_t a, std::uint8_t b ) -> std::uint8_t
                {
                    return a xor b;
                }
            );

            return a;
        }

        bluetoe::details::uint128_t aes( const bluetoe::details::uint128_t& key, bluetoe::details::uint128_t data ) const
        {
            AES_ctx ctx;
            AES_init_ctx( &ctx, &key[ 0 ] );

            AES_ECB_encrypt( &ctx, &data[ 0 ] );

            return data;
        }

        bluetoe::details::uint128_t c1(
            const bluetoe::details::uint128_t& temp_key,
            const bluetoe::details::uint128_t& srand,
            const bluetoe::details::uint128_t& p1,
            const bluetoe::details::uint128_t& p2 ) const
        {
            // c1 (k, r, preq, pres, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
            return aes( temp_key, xor_( aes( temp_key, xor_( srand, p1 ) ), p2 ) );
        }
    };

    template < class Manager, std::size_t MTU = 27 >
    struct security_manager : Manager, private security_functions
    {
        void connect( const bluetoe::link_layer::device_address& addr )
        {
            connection_data_.remote_connection_created( addr );
        }

        void expected(
            std::initializer_list< std::uint8_t > input,
            std::initializer_list< std::uint8_t > expected_output )
        {
            std::uint8_t buffer[ MTU ];
            std::size_t  size = MTU;

            this->l2cap_input( input.begin(), input.size(), &buffer[ 0 ], size,
                connection_data_, static_cast< security_functions& >( *this ) );

            BOOST_CHECK_EQUAL_COLLECTIONS(
                expected_output.begin(), expected_output.end(),
                &buffer[ 0 ], &buffer[ size ] );
        }

        struct gatt_connection_details {};
        using connection_data_t = typename Manager::template connection_data< gatt_connection_details >;

        connection_data_t connection_data_;
    };

    struct pairing_features_exchanged : security_manager< bluetoe::security_manager, 27 >
    {
        pairing_features_exchanged()
        {
            expected(
                {
                    0x01,           // Pairing Request
                    0x03,           // IO Capability NoInputNoOutput
                    0x00,           // OOB data flag (data not present)
                    0x00,           // AuthReq
                    0x10,           // Maximum Encryption Key Size (16)
                    0x00,           // Initiator Key Distribution
                    0x00,           // Responder Key Distribution (RFU)
                },
                {
                    0x02,   // response
                    0x03,   // NoInputNoOutput
                    0x00,   // OOB Authentication data not present
                    0x00,   // Bonding, MITM = 0, SC = 0, Keypress = 0
                    0x10,   // Maximum Encryption Key Size
                    0x00,   // LinkKey
                    0x00    // LinkKey
                }
            );
        }
    };

}

#endif
