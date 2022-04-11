#include <bluetoe/security_tool_box.hpp>
#include <bluetoe/nrf.hpp>

#include "uECC.h"

namespace bluetoe
{
using namespace bluetoe::nrf;

namespace nrf52_details
{
    /////////////////////////////////
    // class security_tool_box

    // TODO: Entropie-Pool to not block here too much
    // How much random numbers do we need:
    //  - without encryption?
    //  - encrytped but already paired? <-- this might be the sweetspot
    //  - leagacy pairing?
    //  - LESC pairing?
    static std::uint8_t random_number8()
    {
        nrf_random->TASKS_START = 1;

        while ( !nrf_random->EVENTS_VALRDY )
            ;

        nrf_random->EVENTS_VALRDY = 0;

        return nrf_random->VALUE;
    }

    static std::uint16_t random_number16()
    {
        return static_cast< std::uint16_t >( random_number8() )
            | ( static_cast< std::uint16_t >( random_number8() ) << 8 );
    }

    std::uint32_t random_number32()
    {
        return static_cast< std::uint32_t >( random_number16() )
            | ( static_cast< std::uint32_t >( random_number16() ) << 16 );
    }

    std::uint64_t random_number64()
    {
        return static_cast< std::uint64_t >( random_number32() )
            | ( static_cast< std::uint64_t >( random_number32() ) << 32 );
    }

    // Note: While every function above the link layer uses low to high byte order inputs to the
    // EAS function, the CCM, that encrypts the link layer trafic, uses high to low byte order.
    // That's why the intput and output in aes_le() changeing the byte order. The result of the
    // key deversification is the key for the CCM algorithm and thus have to be stored in high to
    // low byte order.

    static bluetoe::details::uint128_t aes_le( const bluetoe::details::uint128_t& key, const std::uint8_t* data )
    {
        static struct alignas( 4 ) ecb_data_t {
            std::uint8_t data[ 3 * 16 ];
        } ecb_scratch_data;

        nrf_aes->ECBDATAPTR = reinterpret_cast< std::uint32_t >( &ecb_scratch_data.data[ 0 ] );

        std::copy( key.rbegin(), key.rend(), &ecb_scratch_data.data[ 0 ] );
        std::reverse_copy( data, data + 16, &ecb_scratch_data.data[ 16 ] );

        nrf_aes->TASKS_STARTECB = 1;

        while ( !nrf_aes->EVENTS_ENDECB && !nrf_aes->EVENTS_ERRORECB )
            ;

        assert( !nrf_aes->EVENTS_ERRORECB );
        nrf_aes->EVENTS_ENDECB = 0;

        bluetoe::details::uint128_t result;
        std::copy( &ecb_scratch_data.data[ 32 ], &ecb_scratch_data.data[ 48 ], result.rbegin() );

        // erase key out of memory
        std::fill( &ecb_scratch_data.data[ 0 ], &ecb_scratch_data.data[ 16 ], 0 );

        return result;
    }

    bluetoe::details::uint128_t aes_le( const bluetoe::details::uint128_t& key, const bluetoe::details::uint128_t& data )
    {
        return aes_le( key, data.data() );
    }

    static bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const std::uint8_t* b )
    {
        std::transform(
            a.begin(), a.end(),
            b,
            a.begin(),
            []( std::uint8_t x, std::uint8_t y ) -> std::uint8_t
            {
                return x xor y;
            }
        );

        return a;
    }

    static bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const bluetoe::details::uint128_t& b )
    {
        return xor_( a, b.data() );
    }

    bluetoe::details::uint128_t security_tool_box::create_srand()
    {
        details::uint128_t result;
        std::generate( result.begin(), result.end(), random_number8 );

        return result;
    }

    bluetoe::details::longterm_key_t security_tool_box::create_long_term_key()
    {
        const details::longterm_key_t result = {
            create_srand(),
            random_number64(),
            random_number16()
        };

        return result;
    }

    bluetoe::details::uint128_t security_tool_box::c1(
        const bluetoe::details::uint128_t& temp_key,
        const bluetoe::details::uint128_t& rand,
        const bluetoe::details::uint128_t& p1,
        const bluetoe::details::uint128_t& p2 ) const
    {
        // c1 (k, r, preq, pres, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
        const auto p1_ = aes_le( temp_key, xor_( rand, p1 ) );

        return aes_le( temp_key, xor_( p1_, p2 ) );
    }

    bluetoe::details::uint128_t security_tool_box::s1(
        const bluetoe::details::uint128_t& temp_key,
        const bluetoe::details::uint128_t& srand,
        const bluetoe::details::uint128_t& mrand )
    {
        bluetoe::details::uint128_t r;
        std::copy( &srand[ 0 ], &srand[ 8 ], &r[ 8 ] );
        std::copy( &mrand[ 0 ], &mrand[ 8 ], &r[ 0 ] );

        return aes_le( temp_key, r );
    }

    bool security_tool_box::is_valid_public_key( const std::uint8_t* public_key ) const
    {
        bluetoe::details::ecdh_public_key_t key;
        std::reverse_copy(public_key, public_key + 32, key.begin());
        std::reverse_copy(public_key + 32, public_key + 64, key.begin() + 32);

        return uECC_valid_public_key( key.data() );
    }

    std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > security_tool_box::generate_keys()
    {
        uECC_set_rng( []( uint8_t *dest, unsigned size )->int {
            std::generate( dest, dest + size, random_number8 );

            return 1;
        } );

        bluetoe::details::ecdh_public_key_t  public_key;
        bluetoe::details::ecdh_private_key_t private_key;

        const auto rc = uECC_make_key( public_key.data(), private_key.data() );
        static_cast< void >( rc );
        assert( rc == 1 );

        std::reverse( public_key.begin(), public_key.begin() + 32 );
        std::reverse( public_key.begin() + 32, public_key.end() );
        std::reverse( private_key.begin(), private_key.end() );

        return { public_key, private_key };
    }

    bluetoe::details::uint128_t security_tool_box::select_random_nonce()
    {
        bluetoe::details::uint128_t result;
        std::generate( result.begin(), result.end(), random_number8 );

        return result;
    }

    bluetoe::details::ecdh_shared_secret_t security_tool_box::p256( const std::uint8_t* private_key, const std::uint8_t* public_key )
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
        static_assert(shared_secret.size() == result.size(), "");

        std::reverse_copy( shared_secret.begin(), shared_secret.end(), result.begin() );

        return result;
    }

    static bluetoe::details::uint128_t left_shift(const bluetoe::details::uint128_t& input)
    {
        bluetoe::details::uint128_t output;

        static_assert(input.size() == output.size(), "");

        std::uint8_t overflow = 0;
        for ( std::size_t i = 0; i != input.size(); ++i )
        {
            output[ i ] = ( input[i] << 1 ) | overflow;
            overflow = ( input[ i ] & 0x80 ) ? 1 : 0;
        }

        return output;
    }

    static bluetoe::details::uint128_t aes_cmac_k1_subkey_generation( const bluetoe::details::uint128_t& key )
    {
        const bluetoe::details::uint128_t zero = {{ 0x00 }};
        const bluetoe::details::uint128_t C    = {{ 0x87 }};

        const bluetoe::details::uint128_t k0 = aes_le( key, zero );

        const bluetoe::details::uint128_t k1 = ( k0.back() & 0x80 ) == 0
            ? left_shift(k0)
            : xor_( left_shift(k0), C );

        return k1;
    }

    static bluetoe::details::uint128_t aes_cmac_k2_subkey_generation( const bluetoe::details::uint128_t& key )
    {
        const bluetoe::details::uint128_t C    = {{ 0x87 }};

        const bluetoe::details::uint128_t k1 = aes_cmac_k1_subkey_generation( key );
        const bluetoe::details::uint128_t k2 = ( k1.back() & 0x80 ) == 0
            ? left_shift(k1)
            : xor_( left_shift(k1), C );

        return k2;
    }

    bluetoe::details::uint128_t security_tool_box::f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z )
    {
        const bluetoe::details::uint128_t m4 = {{
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, z
        }};

        auto t0 = aes_le( k, &u[16] );
        auto t1 = aes_le( k, xor_( t0, &u[0] ) );
        auto t2 = aes_le( k, xor_( t1, &v[16] ) );
        auto t3 = aes_le( k, xor_( t2, &v[0] ) );

        return aes_le( k, xor_( t3, xor_( aes_cmac_k2_subkey_generation( k ), m4 ) ) );
    }

    static bluetoe::details::uint128_t f5_cmac(
        const bluetoe::details::uint128_t& key,
        const std::uint8_t* buffer )
    {
        const std::uint8_t* m0 = &buffer[ 48 ];
        const std::uint8_t* m1 = &buffer[ 32 ];
        const std::uint8_t* m2 = &buffer[ 16 ];
        const std::uint8_t* m3 = &buffer[ 0 ];

        auto t0 = aes_le( key, m0 );
        auto t1 = aes_le( key, xor_( t0, m1 ) );
        auto t2 = aes_le( key, xor_( t1, m2 ) );

        return aes_le( key, xor_( t2, xor_( aes_cmac_k2_subkey_generation( key ), m3 ) ) );
    }

    static bluetoe::details::uint128_t f5_key( const bluetoe::details::ecdh_shared_secret_t dh_key )
    {
        static const bluetoe::details::uint128_t salt = {{
            0xBE, 0x83, 0x60, 0x5A, 0xDB, 0x0B, 0x37, 0x60,
            0x38, 0xA5, 0xF5, 0xAA, 0x91, 0x83, 0x88, 0x6C
        }};

        auto t0 = aes_le( salt, &dh_key[ 16 ] );

        return aes_le( salt, xor_( t0, xor_( aes_cmac_k1_subkey_generation( salt ), &dh_key[ 0 ] ) ) );
    }

    std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > security_tool_box::f5(
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

    bluetoe::details::uint128_t security_tool_box::f6(
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

        auto t0 = aes_le( key, n1 );
        auto t1 = aes_le( key, xor_( t0, n2 ) );
        auto t2 = aes_le( key, xor_( t1, r ) );
        auto t3 = aes_le( key, xor_( t2, m3 ) );

        return aes_le( key, xor_( t3, xor_( aes_cmac_k2_subkey_generation( key ), m4 ) ) );
    }

    std::uint32_t security_tool_box::g2(
        const std::uint8_t*                 u,
        const std::uint8_t*                 v,
        const bluetoe::details::uint128_t&  x,
        const bluetoe::details::uint128_t&  y )
    {
        auto t0 = aes_le( x, u + 16 );
        auto t1 = aes_le( x, xor_( t0, u ) );
        auto t2 = aes_le( x, xor_( t1, v + 16 ) );
        auto t3 = aes_le( x, xor_( t2, v ) );
        auto t4 = aes_le( x, xor_( t3, xor_( aes_cmac_k1_subkey_generation( x ), y ) ) );

        return bluetoe::details::read_32bit( t4.begin() );
    }

    bluetoe::details::uint128_t security_tool_box::create_passkey()
    {
        const bluetoe::details::uint128_t result{{
            random_number8(), random_number8(), random_number8()
        }};

        return result;
    }

}
}
