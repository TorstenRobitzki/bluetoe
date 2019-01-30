#ifndef BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP
#define BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP

#include "aes.h"
#include <bluetoe/link_state.hpp>
#include <bluetoe/security_manager.hpp>
#include <bluetoe/address.hpp>

namespace test {

    struct security_functions {
        bluetoe::link_layer::device_address local_address() const
        {
            return local_addr_;
        }

        void local_address( const bluetoe::link_layer::device_address& addr )
        {
            local_addr_ = addr;
        }

        bluetoe::details::uint128_t create_srand()
        {
            const bluetoe::details::uint128_t r{{
                0xE0, 0x2E, 0x70, 0xC6,
                0x4E, 0x27, 0x88, 0x63,
                0x0E, 0x6F, 0xAD, 0x56,
                0x21, 0xD5, 0x83, 0x57
            }};

            return r;
        }

        bluetoe::details::longterm_key_t create_long_term_key()
        {
            bluetoe::details::longterm_key_t key = {
                {{
                    0x00, 0x11, 0x22, 0x33,
                    0x44, 0x55, 0x66, 0x77,
                    0x88, 0x99, 0xaa, 0xbb,
                    0xcc, 0xdd, 0xee, 0xff
                }},
                0xaabbccdd00112233,
                0x1234
            };

            return key;
        }

        // Key generation function s1 for LE Legacy Pairing
        bluetoe::details::uint128_t s1(
            const bluetoe::details::uint128_t& stk,
            const bluetoe::details::uint128_t& srand,
            const bluetoe::details::uint128_t& mrand )
        {
            bluetoe::details::uint128_t r;
            std::copy( &srand[ 0 ], &srand[ 8 ], &r[ 8 ] );
            std::copy( &mrand[ 0 ], &mrand[ 8 ], &r[ 0 ] );

            return aes( stk, r );
        }

        bluetoe::details::uint128_t c1(
            const bluetoe::details::uint128_t& temp_key,
            const bluetoe::details::uint128_t& srand,
            const bluetoe::details::uint128_t& p1,
            const bluetoe::details::uint128_t& p2 ) const
        {
            // c1 (k, r, preq, pres, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
            const auto p1_ = aes( temp_key, xor_( srand, p1 ) );

            return aes( temp_key, xor_( p1_, p2 ) );
        }

    protected:
        bluetoe::details::uint128_t r( const bluetoe::details::uint128_t& a ) const
        {
            const bluetoe::details::uint128_t result{{
                a[15], a[14], a[13], a[12],
                a[11], a[10], a[ 9], a[ 8],
                a[ 7], a[ 6], a[ 5], a[ 4],
                a[ 3], a[ 2], a[ 1], a[ 0],
            }};

            return result;
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

        bluetoe::details::uint128_t aes( bluetoe::details::uint128_t key, bluetoe::details::uint128_t data ) const
        {
            key  = r( key );
            data = r( data );

            AES_ctx ctx;
            AES_init_ctx( &ctx, &key[ 0 ] );

            AES_ECB_encrypt( &ctx, &data[ 0 ] );

            return r( data );
        }

        bluetoe::link_layer::device_address local_addr_;
    };

    template < class Manager, std::size_t MTU = 27 >
    struct security_manager : Manager, private security_functions
    {
        security_manager()
            : connection_data_( MTU )
        {
            local_address(
                bluetoe::link_layer::public_device_address({
                    0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1
                })
            );

            remote_address(
                bluetoe::link_layer::random_device_address({
                    0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1
                })
            );
        }

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
        using connection_data_t = bluetoe::details::link_state<
            typename Manager::template connection_data< gatt_connection_details > >;

        void local_address( const bluetoe::link_layer::device_address& addr )
        {
            static_cast< security_functions& >( *this ).local_address( addr );
        }

        void remote_address( const bluetoe::link_layer::device_address& addr )
        {
            connection_data_.remote_connection_created( addr );
        }

        const connection_data_t& connection_data() const
        {
            return connection_data_;

        }
        connection_data_t connection_data_;
    };

    struct pairing_features_exchanged : security_manager< bluetoe::security_manager, 27 >
    {
        pairing_features_exchanged()
        {
            expected(
                {
                    0x01,           // Pairing Request
                    0x01,           // IO Capability NoInputNoOutput
                    0x00,           // OOB data flag (data not present)
                    0x00,           // AuthReq
                    0x10,           // Maximum Encryption Key Size (16)
                    0x07,           // Initiator Key Distribution
                    0x07,           // Responder Key Distribution (RFU)
                },
                {
                    0x02,           // response
                    0x03,           // NoInputNoOutput
                    0x00,           // OOB Authentication data not present
                    0x00,           // Bonding, MITM = 0, SC = 0, Keypress = 0
                    0x10,           // Maximum Encryption Key Size
                    0x00,           // LinkKey
                    0x00            // LinkKey
                }
            );
        }
    };

    struct pairing_confirm_exchanged : pairing_features_exchanged
    {
        pairing_confirm_exchanged()
        {
            expected(
                {
                    0x03,                   // Pairing Confirm
                    0xe2, 0x16, 0xb5, 0x44, // Confirm Value
                    0x96, 0xa2, 0xe7, 0x90,
                    0x53, 0xb2, 0x31, 0x06,
                    0xc9, 0xdd, 0xd4, 0xf8
                },
                {
                    0x03,                   // Pairing Confirm
                    0xe2, 0x16, 0xb5, 0x44, // Confirm Value
                    0x96, 0xa2, 0xe7, 0x90,
                    0x53, 0xb2, 0x31, 0x06,
                    0xc9, 0xdd, 0xd4, 0xf8
                }
            );
        }
    };

    struct pairing_random_exchanged : pairing_confirm_exchanged
    {
        pairing_random_exchanged()
        {
            expected(
                {
                    0x04,                   // opcode
                    0xE0, 0x2E, 0x70, 0xC6,
                    0x4E, 0x27, 0x88, 0x63,
                    0x0E, 0x6F, 0xAD, 0x56,
                    0x21, 0xD5, 0x83, 0x57
                },
                {
                    0x04,                   // opcode
                    0xE0, 0x2E, 0x70, 0xC6,
                    0x4E, 0x27, 0x88, 0x63,
                    0x0E, 0x6F, 0xAD, 0x56,
                    0x21, 0xD5, 0x83, 0x57
                }
            );
        }
    };

}

#endif
