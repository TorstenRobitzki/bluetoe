#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>
#include "test_sm.hpp"

/**
 * using the example data from Core (V5), Vol 3, Part H, 2.2.3 Confirm value generation function c1 for LE Legacy Pairing
 * p1 is 0x05000800000302070710000001010001
 * p2 is 0x00000000A1A2A3A4A5A6B1B2B3B4B5B6
 * 128-bit k is 0x00000000000000000000000000000000
 * 128-bit value r is 0x5783D52156AD6F0E6388274EC6702EE0
 * 128-bit output from the c1 function is 0x1e1e3fef878988ead2a74dc5bef13b86
 */
BOOST_FIXTURE_TEST_CASE( c1_test, test::legacy_security_functions )
{
    const bluetoe::details::uint128_t p1{{
        0x01, 0x00, 0x01, 0x01,
        0x00, 0x00, 0x10, 0x07,
        0x07, 0x02, 0x03, 0x00,
        0x00, 0x08, 0x00, 0x05
    }};

    const bluetoe::details::uint128_t p2{{
        0xB6, 0xB5, 0xB4, 0xB3,
        0xB2, 0xB1, 0xA6, 0xA5,
        0xA4, 0xA3, 0xA2, 0xA1,
        0x00, 0x00, 0x00, 0x00
    }};

    const bluetoe::details::uint128_t k{{
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }};

    const bluetoe::details::uint128_t r{{
        0xE0, 0x2E, 0x70, 0xC6,
        0x4E, 0x27, 0x88, 0x63,
        0x0E, 0x6F, 0xAD, 0x56,
        0x21, 0xD5, 0x83, 0x57
    }};

    const bluetoe::details::uint128_t expected{{
        0x86, 0x3b, 0xf1, 0xbe,
        0xc5, 0x4d, 0xa7, 0xd2,
        0xea, 0x88, 0x89, 0x87,
        0xef, 0x3f, 0x1e, 0x1e
    }};

    const bluetoe::details::uint128_t confirm = c1( k, r, p1, p2 );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        confirm.begin(), confirm.end(), expected.begin(), expected.end() );
}

// For example if the 128-bit value r1 is 0x000F0E0D0C0B0A091122334455667788 then r1’ is 0x1122334455667788.
// If the 128-bit value r2 is 0x010203040506070899AABBCCDDEEFF00 then r2’ is 0x99AABBCCDDEEFF00.
// For example, if the 64-bit value r1’ is 0x1122334455667788 and r2’ is 0x99AABBCCDDEEFF00 then
// r’ is 0x112233445566778899AABBCCDDEEFF00.
// For example if the 128-bit value k is 0x00000000000000000000000000000000
// and the 128-bit value r' is 0x112233445566778899AABBCCDDEEFF00
// then the output from the key generation function s1 is 0x9a1fe1f0e8b0f49b5b4216ae796da062.
BOOST_FIXTURE_TEST_CASE( s1_test, test::legacy_security_functions )
{
    static const bluetoe::details::uint128_t k = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // r1 = 0x000F0E0D0C0B0A091122334455667788
    static const bluetoe::details::uint128_t r1 = {
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00
    };

    // r2 = 0x010203040506070899AABBCCDDEEFF00
    static const bluetoe::details::uint128_t r2 = {
        0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
    };

    // 0x9a1fe1f0e8b0f49b5b4216ae796da062
    static const bluetoe::details::uint128_t expected = {
        0x62, 0xa0, 0x6d, 0x79, 0xae, 0x16, 0x42, 0x5b,
        0x9b, 0xf4, 0xb0, 0xe8, 0xf0, 0xe1, 0x1f, 0x9a
    };

    const bluetoe::details::uint128_t key = s1( k, r1, r2 );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        key.begin(), key.end(), expected.begin(), expected.end() );
}

BOOST_FIXTURE_TEST_CASE( aes_test, test::legacy_security_functions )
{
    const bluetoe::details::uint128_t key{{
        0x0f, 0x0e, 0x0d, 0x0c,
        0x0b, 0x0a, 0x09, 0x08,
        0x07, 0x06, 0x05, 0x04,
        0x03, 0x02, 0x01, 0x00
    }};

    const bluetoe::details::uint128_t input{{
        0xff, 0xee, 0xdd, 0xcc,
        0xbb, 0xaa, 0x99, 0x88,
        0x77, 0x66, 0x55, 0x44,
        0x33, 0x22, 0x11, 0x00
    }};

    const bluetoe::details::uint128_t expected{{
        0x5a, 0xc5, 0xb4, 0x70,
        0x80, 0xb7, 0xcd, 0xd8,
        0x30, 0x04, 0x7b, 0x6a,
        0xd8, 0xe0, 0xc4, 0x69
    }};

    const bluetoe::details::uint128_t output = aes( key, input );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        output.begin(), output.end(), expected.begin(), expected.end() );
}

BOOST_FIXTURE_TEST_CASE( xor_test, test::legacy_security_functions )
{
    const bluetoe::details::uint128_t p1{{
        0x01, 0x00, 0x01, 0x01,
        0x00, 0x00, 0x10, 0x07,
        0x07, 0x02, 0x03, 0x00,
        0x00, 0x08, 0x00, 0x05
    }};

    const bluetoe::details::uint128_t r{{
        0xE0, 0x2E, 0x70, 0xC6,
        0x4E, 0x27, 0x88, 0x63,
        0x0E, 0x6F, 0xAD, 0x56,
        0x21, 0xD5, 0x83, 0x57
    }};

    const bluetoe::details::uint128_t expected{{
        0xe1, 0x2e, 0x71, 0xc7,
        0x4e, 0x27, 0x98, 0x64,
        0x09, 0x6d, 0xae, 0x56,
        0x21, 0xdd, 0x83, 0x52
    }};

    const bluetoe::details::uint128_t output = xor_( p1, r );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        output.begin(), output.end(), expected.begin(), expected.end() );
}

BOOST_FIXTURE_TEST_CASE( left_shift_tests, test::lesc_security_functions )
{
    const bluetoe::details::uint128_t input = {{
        0x01, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0xFF, 0x00, 0x00, 0x00,
        0x04, 0x40, 0x00, 0xFF
    }};

    const bluetoe::details::uint128_t expected = {{
        0x02, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0xFE, 0x01, 0x00, 0x00,
        0x08, 0x80, 0x00, 0xFE
    }};

    const bluetoe::details::uint128_t output = left_shift( input );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        output.begin(), output.end(), expected.begin(), expected.end() );
}

BOOST_FIXTURE_TEST_CASE( k2_subkey_generation, test::lesc_security_functions )
{
    // --------------------------------------------------
    // Subkey Generation
    // K              2b7e1516 28aed2a6 abf71588 09cf4f3c
    // AES-128(key,0) 7df76b0c 1ab899b3 3e42f047 b91b546f
    // K1             fbeed618 35713366 7c85e08f 7236a8de
    // K2             f7ddac30 6ae266cc f90bc11e e46d513b
    // --------------------------------------------------

    const bluetoe::details::uint128_t key = {{
        0x3c, 0x4f, 0xcf, 0x09,
        0x88, 0x15, 0xf7, 0xab,
        0xa6, 0xd2, 0xae, 0x28,
        0x16, 0x15, 0x7e, 0x2b
    }};

    const bluetoe::details::uint128_t expected = {{
        0x3b, 0x51, 0x6d, 0xe4,
        0x1e, 0xc1, 0x0b, 0xf9,
        0xcc, 0x66, 0xe2, 0x6a,
        0x30, 0xac, 0xdd, 0xf7
    }};

    const bluetoe::details::uint128_t output = aes_cmac_k2_subkey_generation( key );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        output.begin(), output.end(), expected.begin(), expected.end() );
}

BOOST_FIXTURE_TEST_CASE( f4_test, test::lesc_security_functions )
{
    // 4 LE SC CONFIRM VALUE GENERATION FUNCTION
    // U              20b003d2 f297be2c 5e2c83a7 e9f9a5b9
    //                eff49111 acf4fddb cc030148 0e359de6
    // V              55188b3d 32f6bb9a 900afcfb eed4e72a
    //                59cb9ac2 f19d7cfb 6b4fdd49 f47fc5fd
    // X              d5cb8454 d177733e ffffb2ec 712baeab
    // Z 0x00
    // M0             20b003d2 f297be2c 5e2c83a7 e9f9a5b9
    // M1             eff49111 acf4fddb cc030148 0e359de6
    // M2             55188b3d 32f6bb9a 900afcfb eed4e72a
    // M3             59cb9ac2 f19d7cfb 6b4fdd49 f47fc5fd
    // 00
    // AES_CMAC       f2c916f1 07a9bd1c f1eda1be a974872d
    const std::array< std::uint8_t, 32 > u = {{
        0xe6, 0x9d, 0x35, 0x0e,
        0x48, 0x01, 0x03, 0xcc,
        0xdb, 0xfd, 0xf4, 0xac,
        0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9,
        0xa7, 0x83, 0x2c, 0x5e,
        0x2c, 0xbe, 0x97, 0xf2,
        0xd2, 0x03, 0xb0, 0x20
    }};

    const std::array< std::uint8_t, 32 > v = {{
        0xfd, 0xc5, 0x7f, 0xf4,
        0x49, 0xdd, 0x4f, 0x6b,
        0xfb, 0x7c, 0x9d, 0xf1,
        0xc2, 0x9a, 0xcb, 0x59,
        0x2a, 0xe7, 0xd4, 0xee,
        0xfb, 0xfc, 0x0a, 0x90,
        0x9a, 0xbb, 0xf6, 0x32,
        0x3d, 0x8b, 0x18, 0x55
    }};

    const bluetoe::details::uint128_t x = {{
        0xab, 0xae, 0x2b, 0x71,
        0xec, 0xb2, 0xff, 0xff,
        0x3e, 0x73, 0x77, 0xd1,
        0x54, 0x84, 0xcb, 0xd5
    }};

    const std::uint8_t z = 0x00;

    const bluetoe::details::uint128_t expected{{
        0x2d, 0x87, 0x74, 0xa9,
        0xbe, 0xa1, 0xed, 0xf1,
        0x1c, 0xbd, 0xa9, 0x07,
        0xf1, 0x16, 0xc9, 0xf2
    }};

    const bluetoe::details::uint128_t output = f4( u.data(), v.data(), x, z );

    BOOST_CHECK_EQUAL_COLLECTIONS(
        output.begin(), output.end(), expected.begin(), expected.end() );
}
