#ifndef BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP
#define BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP

#include "aes.h"
#include "uECC.h"
#include <bluetoe/link_state.hpp>
#include <bluetoe/security_manager.hpp>
#include <bluetoe/address.hpp>

namespace test {
    struct security_functions_base
    {
        bluetoe::link_layer::device_address local_address() const
        {
            return local_addr_;
        }

        void local_address( const bluetoe::link_layer::device_address& addr )
        {
            local_addr_ = addr;
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

        bluetoe::details::uint128_t aes( bluetoe::details::uint128_t key, const std::uint8_t* data ) const
        {
            bluetoe::details::uint128_t input;
            std::copy(data, data + 16u, input.begin());

            return aes( key, input );
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

        bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const std::uint8_t* b ) const
        {
            std::transform(
                a.begin(), a.end(),
                b,
                a.begin(),
                []( std::uint8_t a, std::uint8_t b ) -> std::uint8_t
                {
                    return a xor b;
                }
            );

            return a;
        }
    private:
        bluetoe::link_layer::device_address local_addr_;
    };

    struct legacy_security_functions : security_functions_base
    {
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

        bluetoe::details::uint128_t create_passkey()
        {
            const bluetoe::details::uint128_t r{{
                0xC7, 0x4c, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
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

    };

    struct lesc_security_functions : security_functions_base
    {
        bool is_valid_public_key( const std::uint8_t* public_key ) const
        {
            bluetoe::details::ecdh_public_key_t key;
            std::reverse_copy(public_key, public_key + 32, key.begin());
            std::reverse_copy(public_key + 32, public_key + 64, key.begin() + 32);

            return uECC_valid_public_key( key.data() );
        }

        std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys()
        {
            static const bluetoe::details::ecdh_public_key_t public_key = {
                {
                    // Public Key X
                    0x90, 0xa1, 0xaa, 0x2f, 0xb2, 0x77, 0x90, 0x55,
                    0x9f, 0xa6, 0x15, 0x86, 0xfd, 0x8a, 0xb5, 0x47,
                    0x00, 0x4c, 0x9e, 0xf1, 0x84, 0x22, 0x59, 0x09,
                    0x96, 0x1d, 0xaf, 0x1f, 0xf0, 0xf0, 0xa1, 0x1e,
                    // Public Key Y
                    0x4a, 0x21, 0xb1, 0x15, 0xf9, 0xaf, 0x89, 0x5f,
                    0x76, 0x36, 0x8e, 0xe2, 0x30, 0x11, 0x2d, 0x47,
                    0x60, 0x51, 0xb8, 0x9a, 0x3a, 0x70, 0x56, 0x73,
                    0x37, 0xad, 0x9d, 0x42, 0x3e, 0xf3, 0x55, 0x4c
                }
            };

            static const bluetoe::details::ecdh_private_key_t private_key = {
                {
                    0xfd, 0xc5, 0x7f, 0xf4, 0x49, 0xdd, 0x4f, 0x6b,
                    0xfb, 0x7c, 0x9d, 0xf1, 0xc2, 0x9a, 0xcb, 0x59,
                    0x2a, 0xe7, 0xd4, 0xee, 0xfb, 0xfc, 0x0a, 0x90,
                    0x9a, 0xbb, 0xf6, 0x32, 0x3d, 0x8b, 0x18, 0x55
                }
            };

            return { public_key, private_key };
        }

        bluetoe::details::uint128_t left_shift(const bluetoe::details::uint128_t& input)
        {
            bluetoe::details::uint128_t output;

            std::uint8_t overflow = 0;
            for ( std::size_t i = 0; i != input.size(); ++i )
            {
                output[ i ] = ( input[i] << 1 ) | overflow;
                overflow = ( input[ i ] & 0x80 ) ? 1 : 0;
            }

            return output;
        }

        bluetoe::details::uint128_t aes_cmac_k1_subkey_generation( const bluetoe::details::uint128_t& key )
        {
            const bluetoe::details::uint128_t zero = {{ 0x00 }};
            const bluetoe::details::uint128_t C    = {{ 0x87 }};

            const bluetoe::details::uint128_t k0 = aes( key, zero );

            const bluetoe::details::uint128_t k1 = ( k0.back() & 0x80 ) == 0
                ? left_shift(k0)
                : xor_( left_shift(k0), C );

            return k1;
        }

        bluetoe::details::uint128_t aes_cmac_k2_subkey_generation( const bluetoe::details::uint128_t& key )
        {
            const bluetoe::details::uint128_t C    = {{ 0x87 }};

            const bluetoe::details::uint128_t k1 = aes_cmac_k1_subkey_generation( key );
            const bluetoe::details::uint128_t k2 = ( k1.back() & 0x80 ) == 0
                ? left_shift(k1)
                : xor_( left_shift(k1), C );

            return k2;
        }

        bluetoe::details::uint128_t select_random_nonce()
        {
            static const bluetoe::details::uint128_t nonce = {{
                0xab, 0xae, 0x2b, 0x71,
                0xec, 0xb2, 0xff, 0xff,
                0x3e, 0x73, 0x77, 0xd1,
                0x54, 0x84, 0xcb, 0xd5
            }};

            return nonce;
        }

        bluetoe::details::ecdh_shared_secret_t p256( const std::uint8_t* private_key, const std::uint8_t* public_key )
        {
            bluetoe::details::ecdh_private_key_t shared_secret;
            bluetoe::details::ecdh_private_key_t priv_key;
            bluetoe::details::ecdh_public_key_t  pub_key;
            std::reverse_copy( public_key, public_key + 32, pub_key.begin() );
            std::reverse_copy( public_key + 32, public_key + 64, pub_key.begin() + 32 );
            std::reverse_copy( private_key, private_key + 32, priv_key.begin() );

            const int rc = uECC_shared_secret( pub_key.data(), priv_key.data(), shared_secret.data() );
            static_cast< void >( rc );
            assert( rc == 1 );

            bluetoe::details::ecdh_shared_secret_t result;
            std::reverse_copy( shared_secret.begin(), shared_secret.end(), result.begin() );

            return result;
        }

        bluetoe::details::uint128_t f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z )
        {
            const bluetoe::details::uint128_t m4 = {{
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x80, z
            }};

            auto t0 = aes( k, &u[16] );
            auto t1 = aes( k, xor_( t0, &u[0] ) );
            auto t2 = aes( k, xor_( t1, &v[16] ) );
            auto t3 = aes( k, xor_( t2, &v[0] ) );

            return aes( k, xor_( t3, xor_( aes_cmac_k2_subkey_generation( k ), m4 ) ) );
        }

        std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
            const bluetoe::details::ecdh_shared_secret_t dh_key,
            const bluetoe::details::uint128_t& nonce_central,
            const bluetoe::details::uint128_t& nonce_periperal,
            const bluetoe::link_layer::device_address& addr_controller,
            const bluetoe::link_layer::device_address& addr_peripheral )
        {
            // all 4 blocks are allocated in revers order to make it easier to copy data that overlaps
            // two blocks
            std::uint8_t buffer[ 64 ] = { 0 };

            static const std::uint8_t m0_fill[] = {
                0x65, 0x6c, 0x74, 0x62
            };

            static const std::uint8_t m3_fill[] = {
                0x80, 0x00, 0x01
            };

            std::copy( std::begin( m0_fill ), std::end( m0_fill ), &buffer[ 11 + 48 ] );
            std::copy( std::begin( m3_fill ), std::end( m3_fill ), &buffer[ 10 ] );

            std::copy( nonce_central.begin(), nonce_central.end(), &buffer[ 32 + 11 ] );
            std::copy( nonce_periperal.begin(), nonce_periperal.end(), &buffer[ 16 + 11 ] );

            buffer[ 16 + 10 ] = addr_controller.is_random() ? 1 : 0;
            std::copy( addr_controller.begin(), addr_controller.end(), &buffer[ 16 + 4 ] );
            buffer[ 16 + 3 ] = addr_peripheral.is_random() ? 1 : 0;
            std::copy( addr_peripheral.begin(), addr_peripheral.end(), &buffer[ 13 ] );

            const bluetoe::details::uint128_t key     = f5_key( dh_key );
            const bluetoe::details::uint128_t mac_key = f5_cmac( key, buffer );
            // increment counter
            buffer[ 15 + 48 ] = 1;
            const bluetoe::details::uint128_t ltk     = f5_cmac( key, buffer );

            return { mac_key, ltk };
        }

        bluetoe::details::uint128_t f6(
            const bluetoe::details::uint128_t& key,
            const bluetoe::details::uint128_t& n1,
            const bluetoe::details::uint128_t& n2,
            const bluetoe::details::uint128_t& r,
            const bluetoe::details::io_capabilities_t& io_caps,
            const bluetoe::link_layer::device_address& addr_controller,
            const bluetoe::link_layer::device_address& addr_peripheral )
        {
            std::uint8_t m4_m3[ 32 ] = { 0 };

            std::copy( io_caps.begin(), io_caps.end(), &m4_m3[ 16 + 13 ] );
            m4_m3[ 16 + 12 ] = addr_controller.is_random() ? 1 : 0;
            std::copy( addr_controller.begin(), addr_controller.end(), &m4_m3[ 22 ] );
            m4_m3[ 16 + 5 ] = addr_peripheral.is_random() ? 1 : 0;
            std::copy( addr_peripheral.begin(), addr_peripheral.end(), &m4_m3[ 15 ] );
            m4_m3[ 14 ] = 0x80;

            const std::uint8_t* m3 = &m4_m3[ 16 ];
            const std::uint8_t* m4 = &m4_m3[ 0 ];

            auto t0 = aes( key, n1 );
            auto t1 = aes( key, xor_( t0, n2 ) );
            auto t2 = aes( key, xor_( t1, r ) );
            auto t3 = aes( key, xor_( t2, m3 ) );

            return aes( key, xor_( t3, xor_( aes_cmac_k2_subkey_generation( key ), m4 ) ) );
        }

    private:
        bluetoe::details::uint128_t f5_cmac(
            const bluetoe::details::uint128_t& key,
            const std::uint8_t* buffer )
        {
            const std::uint8_t* m0 = &buffer[ 48 ];
            const std::uint8_t* m1 = &buffer[ 32 ];
            const std::uint8_t* m2 = &buffer[ 16 ];
            const std::uint8_t* m3 = &buffer[ 0 ];

            auto t0 = aes( key, m0 );
            auto t1 = aes( key, xor_( t0, m1 ) );
            auto t2 = aes( key, xor_( t1, m2 ) );

            return aes( key, xor_( t2, xor_( aes_cmac_k2_subkey_generation( key ), m3 ) ) );
        }

        bluetoe::details::uint128_t f5_key( const bluetoe::details::ecdh_shared_secret_t dh_key )
        {
            static const bluetoe::details::uint128_t salt = {{
                0xBE, 0x83, 0x60, 0x5A, 0xDB, 0x0B, 0x37, 0x60,
                0x38, 0xA5, 0xF5, 0xAA, 0x91, 0x83, 0x88, 0x6C
            }};

            auto t0 = aes( salt, &dh_key[ 16 ] );

            return aes( salt, xor_( t0, xor_( aes_cmac_k1_subkey_generation( salt ), &dh_key[ 0 ] ) ) );
        }
    };

    template < class Manager, class SecurityFunctions, std::size_t MTU = 27, typename ...Options >
    struct security_manager : Manager::template impl< Options... >, private SecurityFunctions
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
            std::vector< std::uint8_t > input,
            std::vector< std::uint8_t > expected_output )
        {
            std::uint8_t buffer[ MTU ];
            std::size_t  size = MTU;

            BOOST_CHECK( !this->security_manager_output_available( connection_data_ ) );

            this->l2cap_input( input.data(), input.size(), &buffer[ 0 ], size,
                connection_data_, static_cast< SecurityFunctions& >( *this ) );

            BOOST_CHECK_EQUAL_COLLECTIONS(
                expected_output.begin(), expected_output.end(),
                &buffer[ 0 ], &buffer[ size ] );
        }

        void expected(
            std::initializer_list< std::uint8_t > in,
            std::initializer_list< std::uint8_t > out )
        {
            std::vector< std::uint8_t > input( in.begin(), in.end() );
            std::vector< std::uint8_t > output( out.begin(), out.end() );

            expected( input, output );
        }

        void expected_ignoring_output_available(
            std::initializer_list< std::uint8_t > input,
            std::initializer_list< std::uint8_t > expected_output )
        {
            std::uint8_t buffer[ MTU ];
            std::size_t  size = MTU;

            this->l2cap_input( input.begin(), input.size(), &buffer[ 0 ], size,
                connection_data_, static_cast< SecurityFunctions& >( *this ) );

            BOOST_CHECK_EQUAL_COLLECTIONS(
                expected_output.begin(), expected_output.end(),
                &buffer[ 0 ], &buffer[ size ] );
        }

        void expected(
            std::initializer_list< std::uint8_t > expected_output )
        {
            std::uint8_t buffer[ MTU ];
            std::size_t  size = MTU;

            BOOST_REQUIRE( this->security_manager_output_available( connection_data_ ) );

            this->l2cap_output( &buffer[ 0 ], size, connection_data_, static_cast< SecurityFunctions& >( *this ) );

            BOOST_CHECK_EQUAL_COLLECTIONS(
                expected_output.begin(), expected_output.end(),
                &buffer[ 0 ], &buffer[ size ] );
        }

        struct gatt_connection_details {};
        using manager_type = typename Manager::template impl< Options... >;
        using connection_data_t = bluetoe::details::link_state<
            typename manager_type::template connection_data< gatt_connection_details > >;

        void local_address( const bluetoe::link_layer::device_address& addr )
        {
            static_cast< SecurityFunctions& >( *this ).local_address( addr );
        }

        void remote_address( const bluetoe::link_layer::device_address& addr )
        {
            connection_data_.remote_connection_created( addr );
        }

        const connection_data_t& connection_data() const
        {
            return connection_data_;
        }

        void expected_pairing_confirm_with_tk( const std::array< std::uint8_t, 16 >& tk_value )
        {
            const bluetoe::details::uint128_t central_pairing_random = {{
                0xE0, 0x2E, 0x70, 0xC6,
                0x4E, 0x27, 0x88, 0x63,
                0x0E, 0x6F, 0xAD, 0x56,
                0x21, 0xD5, 0x83, 0x57
            }};

            std::vector< std::uint8_t > pairing_confirm_request( 17, 0x03 ); // Pairing Confirm
            const auto central_pairing_confirm = this->c1( tk_value, central_pairing_random, connection_data_.c1_p1(), connection_data_.c1_p2() );
            std::copy( central_pairing_confirm.begin(), central_pairing_confirm.end(), &pairing_confirm_request[ 1 ] );

            std::vector< std::uint8_t > pairing_confirm_response( 17, 0x03 ); // Pairing Confirm
            const auto sconfirm = this->c1( tk_value, connection_data_.srand(), connection_data_.c1_p1(), connection_data_.c1_p2() );
            std::copy( sconfirm.begin(), sconfirm.end(), &pairing_confirm_response[ 1 ] );

            // Paring Confirm
            expected( pairing_confirm_request, pairing_confirm_response );

            std::vector< std::uint8_t > pairing_random_request( 17, 0x04 ); // Pairing Random
            std::copy( central_pairing_random.begin(), central_pairing_random.end(), &pairing_random_request[ 1 ] );

            std::vector< std::uint8_t > pairing_random_response( 17, 0x04 ); // Pairing Random
            const auto srand = connection_data_.srand();
            std::copy( srand.begin(), srand.end(), &pairing_random_response[ 1 ] );

            // Paring Random
            expected( pairing_random_request, pairing_random_response );

            const auto expected_stk = this->s1( tk_value, connection_data_.srand(), central_pairing_random );
            const auto stored_stk   = connection_data_.find_key( 0, 0 );

            BOOST_REQUIRE( stored_stk.first );
            BOOST_CHECK_EQUAL_COLLECTIONS( expected_stk.begin(), expected_stk.end(), stored_stk.second.begin(), stored_stk.second.end() );
        }

        connection_data_t connection_data_;
    };

    template < std::size_t MTU = 23, typename ...Options >
    using legacy_security_manager = security_manager< bluetoe::legacy_security_manager, test::legacy_security_functions, MTU, Options... >;

    template < std::size_t MTU = 65, typename ...Options >
    using lesc_security_manager = security_manager< bluetoe::lesc_security_manager, test::lesc_security_functions, MTU, Options... >;

    struct legacy_pairing_features_exchanged : security_manager< bluetoe::legacy_security_manager, legacy_security_functions >
    {
        legacy_pairing_features_exchanged()
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

    struct lesc_pairing_features_exchanged : security_manager< bluetoe::lesc_security_manager, lesc_security_functions, 65 >
    {
        lesc_pairing_features_exchanged()
        {
            expected(
                {
                    0x01,           // Pairing Request
                    0x01,           // IO Capability NoInputNoOutput
                    0x00,           // OOB data flag (data not present)
                    0x08,           // AuthReq, SC = 1
                    0x10,           // Maximum Encryption Key Size (16)
                    0x07,           // Initiator Key Distribution
                    0x07,           // Responder Key Distribution (RFU)
                },
                {
                    0x02,           // response
                    0x03,           // NoInputNoOutput
                    0x00,           // OOB Authentication data not present
                    0x08,           // Bonding, MITM = 0, SC = 1, Keypress = 0
                    0x10,           // Maximum Encryption Key Size
                    0x00,           // LinkKey
                    0x00            // LinkKey
                }
            );
        }
    };

    struct legacy_pairing_confirm_exchanged : legacy_pairing_features_exchanged
    {
        legacy_pairing_confirm_exchanged()
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

    struct legacy_pairing_random_exchanged : legacy_pairing_confirm_exchanged
    {
        legacy_pairing_random_exchanged()
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

    struct lesc_public_key_exchanged : lesc_pairing_features_exchanged
    {
        lesc_public_key_exchanged()
        {
            expected(
                {
                    0x0C,                   // Pairing Public Key
                    // Public Key X
                    0xe6, 0x9d, 0x35, 0x0e,
                    0x48, 0x01, 0x03, 0xcc,
                    0xdb, 0xfd, 0xf4, 0xac,
                    0x11, 0x91, 0xf4, 0xef,
                    0xb9, 0xa5, 0xf9, 0xe9,
                    0xa7, 0x83, 0x2c, 0x5e,
                    0x2c, 0xbe, 0x97, 0xf2,
                    0xd2, 0x03, 0xb0, 0x20,
                    // Public Key Y
                    0x8b, 0xd2, 0x89, 0x15,
                    0xd0, 0x8e, 0x1c, 0x74,
                    0x24, 0x30, 0xed, 0x8f,
                    0xc2, 0x45, 0x63, 0x76,
                    0x5c, 0x15, 0x52, 0x5a,
                    0xbf, 0x9a, 0x32, 0x63,
                    0x6d, 0xeb, 0x2a, 0x65,
                    0x49, 0x9c, 0x80, 0xdc
                },
                {
                    0x0C,                   // Pairing Public Key
                    // Public Key X
                    0x90, 0xa1, 0xaa, 0x2f,
                    0xb2, 0x77, 0x90, 0x55,
                    0x9f, 0xa6, 0x15, 0x86,
                    0xfd, 0x8a, 0xb5, 0x47,
                    0x00, 0x4c, 0x9e, 0xf1,
                    0x84, 0x22, 0x59, 0x09,
                    0x96, 0x1d, 0xaf, 0x1f,
                    0xf0, 0xf0, 0xa1, 0x1e,
                    // Public Key Y
                    0x4a, 0x21, 0xb1, 0x15,
                    0xf9, 0xaf, 0x89, 0x5f,
                    0x76, 0x36, 0x8e, 0xe2,
                    0x30, 0x11, 0x2d, 0x47,
                    0x60, 0x51, 0xb8, 0x9a,
                    0x3a, 0x70, 0x56, 0x73,
                    0x37, 0xad, 0x9d, 0x42,
                    0x3e, 0xf3, 0x55, 0x4c,
                }
            );
        }
    };

    struct lesc_pairing_confirmed : lesc_public_key_exchanged
    {
        lesc_pairing_confirmed()
        {
            expected(
                {
                    0x03,           // Pairing Confirm
                    0x99, 0x57, 0x89, 0x9b,
                    0xaa, 0x41, 0x1b, 0x45,
                    0x15, 0x8d, 0x58, 0xff,
                    0x73, 0x9b, 0x81, 0xd1
                }
            );
        }
    };

    struct lesc_pairing_random_exchanged : lesc_pairing_confirmed
    {
        lesc_pairing_random_exchanged()
        {
            expected(
                {
                    0x04,           // Pairing Random
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00
                },
                {
                    0x04,           // Pairing Random
                    0xab, 0xae, 0x2b, 0x71,
                    0xec, 0xb2, 0xff, 0xff,
                    0x3e, 0x73, 0x77, 0xd1,
                    0x54, 0x84, 0xcb, 0xd5
                }
            );
        }
    };

}

#endif
