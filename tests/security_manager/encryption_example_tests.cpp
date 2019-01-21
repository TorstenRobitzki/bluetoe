#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <array>
#include "aes.h"
#include "../hexdump.hpp"

using uint128_t = std::array< std::uint8_t, 16 >;
using uint64_t_  = std::array< std::uint8_t, 8 >;
using uint32_t_  = std::array< std::uint8_t, 4 >;

static const uint128_t master_random_value = {
    0xde, 0x15, 0x6b, 0xef, 0xb3, 0xd5, 0x8b, 0xdb,
    0x58, 0x11, 0x8f, 0x3d, 0x49, 0xbd, 0x31, 0x0e };

static const uint128_t slave_random_value = {
    0x58, 0x44, 0xa0, 0xd4, 0x88, 0xd8, 0xc4, 0x7d,
    0xd9, 0x51, 0x0e, 0x1d, 0x00, 0xa8, 0x9a, 0x64 };

static const uint128_t short_term_key = {
    0xa8, 0xb4, 0x33, 0x6b, 0x59, 0x91, 0x7e, 0x2c,
    0xab, 0xa9, 0xfd, 0xd7, 0xc3, 0xb5, 0xd7, 0x12
};


// Mconfirm = c1(TK, Mrand,
// Pairing Request command, Pairing Response command, initiating device address type, initiating device address, responding device address type, responding device address)

// Sconfirm = c1(TK, Srand,
// Pairing Request command, Pairing Response command,

static uint128_t xor_( uint128_t a, uint128_t b )
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

uint128_t r( const uint128_t& a )
{
    const uint128_t result{{
        a[15], a[14], a[13], a[12],
        a[11], a[10], a[ 9], a[ 8],
        a[ 7], a[ 6], a[ 5], a[ 4],
        a[ 3], a[ 2], a[ 1], a[ 0],
    }};

    return result;
}

uint128_t aes( uint128_t key, uint128_t data )
{
    AES_ctx ctx;
    AES_init_ctx( &ctx, &key[ 0 ] );

    AES_ECB_encrypt( &ctx, &data[ 0 ] );

    return data;
}

uint128_t aes_le( uint128_t key, uint128_t data )
{
    key  = r( key );
    data = r( data );

    AES_ctx ctx;
    AES_init_ctx( &ctx, &key[ 0 ] );

    AES_ECB_encrypt( &ctx, &data[ 0 ] );

    return r( data );
}

BOOST_AUTO_TEST_CASE( aes_test )
{
    const uint128_t key{{
        0x0f, 0x0e, 0x0d, 0x0c,
        0x0b, 0x0a, 0x09, 0x08,
        0x07, 0x06, 0x05, 0x04,
        0x03, 0x02, 0x01, 0x00
    }};

    const uint128_t input{{
        0xff, 0xee, 0xdd, 0xcc,
        0xbb, 0xaa, 0x99, 0x88,
        0x77, 0x66, 0x55, 0x44,
        0x33, 0x22, 0x11, 0x00
    }};

    const uint128_t expected{{
        0x5a, 0xc5, 0xb4, 0x70,
        0x80, 0xb7, 0xcd, 0xd8,
        0x30, 0x04, 0x7b, 0x6a,
        0xd8, 0xe0, 0xc4, 0x69
    }};

    const uint128_t output = aes_le( key, input );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        output.begin(), output.end(), expected.begin(), expected.end() );
}

static uint128_t c1( uint128_t k, uint128_t r, uint128_t p1, uint128_t p2 )
{
    // c1 (k, r, preq, pres, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
    // p1 = pres || preq || rat’ || iat’
    // p2 = padding || ia || ra
    return aes_le(k, xor_( aes_le(k, xor_(r, p1)), p2) );
}

static void check_equal( uint128_t a, uint128_t b )
{
    BOOST_CHECK_EQUAL_COLLECTIONS( a.begin(), a.end(), b.begin(), b.end() );
}

BOOST_AUTO_TEST_CASE( test_c1 )
{
    // For example, if the 128-bit k is 0x00000000000000000000000000000000,
    // the 128-bit value r is 0x5783D52156AD6F0E6388274EC6702EE0, the 128-bit
    // value p1 is 0x05000800000302070710000001010001 and the 128-bit value
    // p2 is 0x00000000A1A2A3A4A5A6B1B2B3B4B5B6 then the 128-bit output from
    // the c1 function is 0x1e1e3fef878988ead2a74dc5bef13b86.

    static const uint128_t k = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    static const uint128_t r = {
        0xE0, 0x2E, 0x70, 0xC6, 0x4E, 0x27, 0x88, 0x63,
        0x0E, 0x6F, 0xAD, 0x56, 0x21, 0xD5, 0x83, 0x57
    };

    static const uint128_t p1 = {
        0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x10, 0x07,
        0x07, 0x02, 0x03, 0x00, 0x00, 0x08, 0x00, 0x05
    };

    static const uint128_t p2 = {
        0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1, 0xA6, 0xA5,
        0xA4, 0xA3, 0xA2, 0xA1, 0x00, 0x00, 0x00, 0x00
    };

    static const uint128_t expected = {
        0x86, 0x3b, 0xf1, 0xbe, 0xc5, 0x4d, 0xa7, 0xd2,
        0xea, 0x88, 0x89, 0x87, 0xef, 0x3f, 0x1e, 0x1e
    };

    check_equal( c1( k, r, p1, p2 ), expected );
}

// s1(k, r1, r2) = e(k, r’)
static uint128_t s1( uint128_t k, uint128_t r1, uint128_t r2 )
{
    uint128_t r;
    std::copy( r1.begin(), r1.begin() + 8, r.begin() + 8 );
    std::copy( r2.begin(), r2.begin() + 8, r.begin() );

    return aes_le( k, r );
}

BOOST_AUTO_TEST_CASE( test_s1 )
{
    static const uint128_t k = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // r1 = 0x000F0E0D0C0B0A091122334455667788
    static const uint128_t r1 = {
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00
    };

    // r2 = 0x010203040506070899AABBCCDDEEFF00
    static const uint128_t r2 = {
        0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
    };

    // 0x9a1fe1f0e8b0f49b5b4216ae796da062
    static const uint128_t expected = {
        0x62, 0xa0, 0x6d, 0x79, 0xae, 0x16, 0x42, 0x5b,
        0x9b, 0xf4, 0xb0, 0xe8, 0xf0, 0xe1, 0x1f, 0x9a
    };

    check_equal( s1( k, r1, r2 ), expected );
}

BOOST_AUTO_TEST_CASE( check_short_term_key )
{
    static const uint128_t k = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    check_equal( s1( k, slave_random_value, master_random_value ), short_term_key );
}

static uint128_t add_to_mac( uint128_t key, uint128_t mac, uint128_t block )
{
    return aes( key, xor_( mac, block ) );
}

static uint32_t ccm(uint128_t key, uint64_t counter, bool sent_by_master, uint64_t iv, const uint8_t* input_begin, const uint8_t* input_end, uint8_t* output )
{

    static const std::size_t  block_size = 128 / 8;
           const std::size_t  message_length = std::distance( input_begin, input_end );

    assert( message_length > 2 );
    assert( message_length < 256 + 2 );

    uint128_t b_0 = {
        0x49,                                           // Flags as per the CCM specification
        static_cast< std::uint8_t >( counter ),         // Octet0 (LSO) of packetCounter
        static_cast< std::uint8_t >( counter >> 8 ),    // Octet1 of packetCounter
        static_cast< std::uint8_t >( counter >> 16 ),   // Octet2 of packetCounter
        static_cast< std::uint8_t >( counter >> 24 ),   // Octet3 of packetCounter
        static_cast< std::uint8_t >( counter >> 32 |    // Bit 6 – Bit 0: Octet4 (7 most significant bits of packetCounter, with Bit 6 being the most significant bit)
         sent_by_master ? 0x80 : 0x00 ),                // Bit7:directionBit
        static_cast< std::uint8_t >( iv ),              // Octet0 (LSO) of IV
        static_cast< std::uint8_t >( iv >> 8 ),         // Octet1 of IV
        static_cast< std::uint8_t >( iv >> 16 ),        // Octet2 of IV
        static_cast< std::uint8_t >( iv >> 24 ),        // Octet3 of IV
        static_cast< std::uint8_t >( iv >> 32 ),        // Octet4 of IV
        static_cast< std::uint8_t >( iv >> 40 ),        // Octet5 of IV
        static_cast< std::uint8_t >( iv >> 48 ),        // Octet6 of IV
        static_cast< std::uint8_t >( iv >> 56 ),        // Octet7 of IV
        0x00,                                           // The most significant octet of the length of the payload
        static_cast< std::uint8_t >( message_length - 2 ) // The least significant octet of the length of the payload
    };

    const auto x1 = aes( key, b_0 );

    uint128_t b_1 = {
        0x00,                   // The most significant octet of the length of the additional authenticated data
        0x01,                   // The least significant octet of the length of the additional authenticated data
        static_cast< std::uint8_t >( *input_begin & 0x03 ),    // The data channel PDU header’s first octet with NESN, SN and MD bits masked to 0
                                // aka LLID
    };

    uint128_t mac = add_to_mac( key, x1, b_1 );

    // move input from header to payload
    input_begin += 2;

    for ( auto begin = input_begin; begin != input_end; )
    {
        uint128_t block = { 0 };
        const std::size_t count = std::min< std::size_t >( std::distance( begin, input_end ), block_size );

        std::copy( begin, begin + count, &block[ 0 ] );
        mac = add_to_mac( key, mac, block );

        begin += count;
    }

    // now start entcryption b_0 is equal to A_i when setting flags and i
    // flag
    b_0[ 0 ]  = 0x01;
    b_0[ 15 ] = 0;    // i = 0

    const auto s_0 = aes( key, b_0 );

    mac = xor_( mac, s_0 );

    for ( auto begin = input_begin; begin != input_end; )
    {
        ++b_0[ 15 ];
        const auto s_n = aes( key, b_0 );

        const std::size_t count = std::min< std::size_t >( std::distance( begin, input_end ), block_size );

        uint128_t block;
        std::copy( begin, begin + count, &block[ 0 ] );
        block = xor_( block, s_n );

        std::copy( &block[ 0 ], &block[ count ], output );

        output += count;
        begin  += count;
    }

    return
        static_cast< std::uint32_t >( mac[ 3 ] )
      | static_cast< std::uint32_t >( mac[ 2 ] ) << 8
      | static_cast< std::uint32_t >( mac[ 1 ] ) << 16
      | static_cast< std::uint32_t >( mac[ 0 ] ) << 24;
}

/*
BOOST_AUTO_TEST_CASE( test_sdk )
{
    static const uint128_t ltk = {
        0x42, 0xc6, 0x06, 0x98, 0x85, 0xe4, 0x94, 0xeb,
        0xa3, 0xed, 0x19, 0x93, 0xb7, 0x85, 0xd4, 0x99
    };

    static const uint128_t skd = {
        0xf4, 0x1e, 0x34, 0x80, 0x5b, 0x2e, 0x37, 0x6c,
        0x52, 0xf5, 0x07, 0xf8, 0x64, 0xbd, 0x43, 0xca
    };

    static const uint128_t stk = {
        0xc1, 0x91, 0x63, 0xa5, 0xc6, 0x5f, 0x8e, 0x3f,
        0x9b, 0x73, 0xed, 0xdf, 0x17, 0x4c, 0x8c, 0x34
    };

    check_equal( aes( ltk, skd ), stk );
}
*/
BOOST_AUTO_TEST_CASE( test_ccm )
{
    static const uint128_t session_key = {
        0x99, 0xAD, 0x1B, 0x52, 0x26, 0xA3, 0x7E, 0x3E,
        0x05, 0x8E, 0x3B, 0x8E, 0x27, 0xC2, 0xC6, 0x66
    };

    static const uint64_t iv  = 0xDEAFBABEBADCAB24;
    static const uint32_t mic = 0xCDA7F448;

    static const uint8_t input[]        = { 0xf, 0x01, 0x06 };
    static       uint8_t output[ 1 ]    = { 0 };
    static const uint8_t expected[ 1 ]  = { 0x9f };

    const std::uint32_t calc_mic = ccm( session_key, 0, true, iv, std::begin(input), std::end(input), std::begin(output) );

    BOOST_CHECK_EQUAL( calc_mic, mic );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin(output), std::end(output), std::begin(expected), std::end(expected) );
}

/*
BOOST_AUTO_TEST_CASE( test_ccm_with_real_data )
{
    static const uint128_t key = {
        0xc1, 0x91, 0x63, 0xa5, 0xc6, 0x5f, 0x8e, 0x3f,
        0x9b, 0x73, 0xed, 0xdf, 0x17, 0x4c, 0x8c, 0x34
    };

    uint64_t iv  = 0x2656ace7e969a980;
    uint32_t mic = 0x712c7a1a;

    static const uint8_t input[] = { 0xf, 0x1, 0x06 };
    static       uint8_t output[20] = {0};

    const std::uint32_t calc_mic = ccm( key, 0, true, iv, std::begin(input), std::end(input), std::begin(output) );

    BOOST_CHECK_EQUAL( calc_mic, mic );
}
*/