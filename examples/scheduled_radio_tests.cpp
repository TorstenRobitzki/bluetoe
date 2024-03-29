#include <bluetoe/nrf51.hpp>
#include <cassert>
#include <algorithm>
#include <functional>

struct radio_t : bluetoe::nrf51_details::scheduled_radio_factory<
            bluetoe::nrf51_details::scheduled_radio_base_with_encryption<>
        >::template scheduled_radio< 100, 100, radio_t >
{
    void adv_received( const bluetoe::link_layer::read_buffer& )
    {
    }

    bool is_scan_request_in_filter( const bluetoe::link_layer::device_address& ) const
    {
        return true;
    }
};

static bool c1_test()
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

    radio_t radio;
    const bluetoe::details::uint128_t confirm = radio.c1( k, r, p1, p2 );

    return std::distance( confirm.begin(), confirm.end() ) == std::distance( expected.begin(), expected.end() )
        && std::equal( confirm.begin(), confirm.end(), expected.begin() );
}

static bool s1_test()
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

    radio_t radio;

    const bluetoe::details::uint128_t key = radio.s1( k, r1, r2 );

    return std::distance( key.begin(), key.end() ) == std::distance( expected.begin(), expected.end() )
        && std::equal( key.begin(), key.end(), expected.begin() );
}

/*
 * This test does not have a easily oberservable result, but can be used for debugging.


LTK = 0x4C68384139F574D836BCF34E9DFB01BF (MSO to LSO)
EDIV = 0x2474 (MSO to LSO)
RAND = 0xABCDEF1234567890 (MSO to LSO)
SKDm = 0xACBDCEDFE0F10213 (MSO to LSO)
SKDs = 0x0213243546576879 (MSO to LSO)
IVm = 0xBADCAB24 (MSO to LSO)
IVs = 0xDEAFBABE (MSO to LSO)

 */
static bool setup_encryption_test()
{
    static const std::uint64_t SKDm = 0xACBDCEDFE0F10213;
    static const std::uint32_t IVm  = 0xBADCAB24;

    static const bluetoe::details::uint128_t key = {

        0xBF, 0x01, 0xFB, 0x9D, 0x4E, 0xF3, 0xBC, 0x36,
        0xD8, 0x74, 0xF5, 0x39, 0x41, 0x38, 0x68, 0x4C
    };

    radio_t radio;
    std::pair< std::uint64_t, std::uint32_t > r = radio.setup_encryption( key, SKDm, IVm );

    static_cast< void >( r );

    return true;
}

static bool is_valid_public_key_test()
{
    static const bluetoe::details::ecdh_public_key_t valid_key = {{
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
    }};

    static const bluetoe::details::ecdh_public_key_t invalid_key = {{
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
        0x49, 0x9c, 0x80, 0x00
    }};

    radio_t radio;

    return radio.is_valid_public_key( valid_key.data() ) && !radio.is_valid_public_key( invalid_key.data() );
}

template < class K >
static bool find_key_duplicate( const K* begin, const K* end )
{
    if ( begin == end )
        return false;

    const K& key = *begin;
    ++begin;

    for ( ; begin != end; ++begin )
    {
        if ( std::equal( key.first.begin(), key.first.end(), begin->first.begin() )
          or std::equal( key.second.begin(), key.second.end(), begin->second.begin() ) )
            return true;
    }

    return false;
}

template < class KP >
static bool try_create_shared_secret( radio_t& radion, const KP& first_pair, const KP& second_pair )
{
    const bluetoe::details::ecdh_shared_secret_t shared1 = radion.p256( first_pair.second.data(), second_pair.first.data() );
    const bluetoe::details::ecdh_shared_secret_t shared2 = radion.p256( second_pair.second.data(), first_pair.first.data() );

    return std::equal( shared1.begin(), shared1.end(), shared2.begin() );
}

static bool generate_keys_test()
{
    static constexpr std::size_t num_keys = 10;
    std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > keys[ num_keys ];

    radio_t radio;
    std::generate( std::begin( keys ), std::end( keys ), [&]() -> std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t >
    {
        return radio.generate_keys();
    });

    // make sure, all keys are different
    for ( int key_idx = 0; key_idx != num_keys; ++key_idx )
    {
        if ( find_key_duplicate( std::next( std::begin( keys ), key_idx ), std::end( keys ) ) )
            return false;
    }

    // make sure, all public keys are valid
    for ( const auto& key : keys )
    {
        if ( !radio.is_valid_public_key( key.first.data() ) )
            return false;
    }

    // use keys pairwise to generate and compare a shared secret
    static_assert( ( num_keys & 1 )  == 0, "number of key must be odd for this tests" );
    for ( int key_idx = 0; key_idx != num_keys; key_idx += 2 )
    {
        if ( !try_create_shared_secret( radio, keys[ key_idx ], keys[ key_idx + 1 ] ) )
            return false;
    }

    return true;
}

static bool p256_test()
{
    const bluetoe::details::ecdh_private_key_t Private_A = {{
        0xbd, 0x1a, 0x3c, 0xcd, 0xa6, 0xb8, 0x99, 0x58,
        0x99, 0xb7, 0x40, 0xeb, 0x7b, 0x60, 0xff, 0x4a,
        0x50, 0x3f, 0x10, 0xd2, 0xe3, 0xb3, 0xc9, 0x74,
        0x38, 0x5f, 0xc5, 0xa3, 0xd4, 0xf6, 0x49, 0x3f
    }};

    const bluetoe::details::ecdh_private_key_t Private_B = {{
        0xfd, 0xc5, 0x7f, 0xf4, 0x49, 0xdd, 0x4f, 0x6b,
        0xfb, 0x7c, 0x9d, 0xf1, 0xc2, 0x9a, 0xcb, 0x59,
        0x2a, 0xe7, 0xd4, 0xee, 0xfb, 0xfc, 0x0a, 0x90,
        0x9a, 0xbb, 0xf6, 0x32, 0x3d, 0x8b, 0x18, 0x55
    }};

    const bluetoe::details::ecdh_public_key_t Public_A = {{
        0xe6, 0x9d, 0x35, 0x0e, 0x48, 0x01, 0x03, 0xcc,
        0xdb, 0xfd, 0xf4, 0xac, 0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9, 0xa7, 0x83, 0x2c, 0x5e,
        0x2c, 0xbe, 0x97, 0xf2, 0xd2, 0x03, 0xb0, 0x20,

        0x8b, 0xd2, 0x89, 0x15, 0xd0, 0x8e, 0x1c, 0x74,
        0x24, 0x30, 0xed, 0x8f, 0xc2, 0x45, 0x63, 0x76,
        0x5c, 0x15, 0x52, 0x5a, 0xbf, 0x9a, 0x32, 0x63,
        0x6d, 0xeb, 0x2a, 0x65, 0x49, 0x9c, 0x80, 0xdc
    }};

    const bluetoe::details::ecdh_public_key_t Public_B = {{
        0x90, 0xa1, 0xaa, 0x2f, 0xb2, 0x77, 0x90, 0x55,
        0x9f, 0xa6, 0x15, 0x86, 0xfd, 0x8a, 0xb5, 0x47,
        0x00, 0x4c, 0x9e, 0xf1, 0x84, 0x22, 0x59, 0x09,
        0x96, 0x1d, 0xaf, 0x1f, 0xf0, 0xf0, 0xa1, 0x1e,

        0x4a, 0x21, 0xb1, 0x15, 0xf9, 0xaf, 0x89, 0x5f,
        0x76, 0x36, 0x8e, 0xe2, 0x30, 0x11, 0x2d, 0x47,
        0x60, 0x51, 0xb8, 0x9a, 0x3a, 0x70, 0x56, 0x73,
        0x37, 0xad, 0x9d, 0x42, 0x3e, 0xf3, 0x55, 0x4c
    }};

    const bluetoe::details::ecdh_shared_secret_t DHKey = {{
        0x98, 0xa6, 0xbf, 0x73, 0xf3, 0x34, 0x8d, 0x86,
        0xf1, 0x66, 0xf8, 0xb4, 0x13, 0x6b, 0x79, 0x99,
        0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34,
        0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec
    }};

    radio_t radio;

    const auto shared_a = radio.p256( Private_A.data(), Public_B.data() );
    const auto shared_b = radio.p256( Private_B.data(), Public_A.data() );

    return std::equal( DHKey.begin(), DHKey.end(), shared_a.begin(), shared_a.end() )
        && std::equal( DHKey.begin(), DHKey.end(), shared_b.begin(), shared_b.end() );

}

static bool f4_test()
{
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

    radio_t radio;
    const bluetoe::details::uint128_t output = radio.f4( u.data(), v.data(), x, z );

    return std::equal( output.begin(), output.end(), expected.begin() );
}

static bool f5_test()
{
    const bluetoe::details::ecdh_shared_secret_t dh_key = {{
        0x98, 0xa6, 0xbf, 0x73, 0xf3, 0x34, 0x8d, 0x86,
        0xf1, 0x66, 0xf8, 0xb4, 0x13, 0x6b, 0x79, 0x99,
        0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34,
        0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec
    }};

    const bluetoe::details::uint128_t nonce_central = {{
        0xab, 0xae, 0x2b, 0x71, 0xec, 0xb2, 0xff, 0xff,
        0x3e, 0x73, 0x77, 0xd1, 0x54, 0x84, 0xcb, 0xd5
    }};

    const bluetoe::details::uint128_t nonce_periperal = {{
        0xcf, 0xc4, 0x3d, 0xff, 0xf7, 0x83, 0x65, 0x21,
        0x6e, 0x5f, 0xa7, 0x25, 0xcc, 0xe7, 0xe8, 0xa6
    }};

    const bluetoe::link_layer::public_device_address addr_controller({
        0xce, 0xbf, 0x37, 0x37, 0x12, 0x56
    });

    const bluetoe::link_layer::public_device_address addr_peripheral({
        0xc1, 0xcf, 0x2d, 0x70, 0x13, 0xa7
    });

    bluetoe::details::uint128_t mac_key;
    bluetoe::details::uint128_t ltk;

    radio_t radio;
    std::tie( mac_key, ltk ) = radio.f5( dh_key, nonce_central, nonce_periperal, addr_controller, addr_peripheral );

    const bluetoe::details::uint128_t expected_mac_key = {{
        0x20, 0x6e, 0x63, 0xce, 0x20, 0x6a, 0x3f, 0xfd,
        0x02, 0x4a, 0x08, 0xa1, 0x76, 0xf1, 0x65, 0x29
    }};

    const bluetoe::details::uint128_t expected_ltk = {{
        0x38, 0x0a, 0x75, 0x94, 0xb5, 0x22, 0x05, 0x98,
        0x23, 0xcd, 0xd7, 0x69, 0x11, 0x79, 0x86, 0x69
    }};

    return std::equal( mac_key.begin(), mac_key.end(), expected_mac_key.begin() )
       and std::equal( ltk.begin(), ltk.end(), expected_ltk.begin() );

}

static bool f6_test()
{
    static const bluetoe::details::uint128_t nonce_central = {{
        0xab, 0xae, 0x2b, 0x71, 0xec, 0xb2, 0xff, 0xff,
        0x3e, 0x73, 0x77, 0xd1, 0x54, 0x84, 0xcb, 0xd5
    }};

    static const bluetoe::details::uint128_t nonce_periperal = {{
        0xcf, 0xc4, 0x3d, 0xff, 0xf7, 0x83, 0x65, 0x21,
        0x6e, 0x5f, 0xa7, 0x25, 0xcc, 0xe7, 0xe8, 0xa6
    }};

    static const bluetoe::details::uint128_t mac_key = {{
        0x20, 0x6e, 0x63, 0xce, 0x20, 0x6a, 0x3f, 0xfd,
        0x02, 0x4a, 0x08, 0xa1, 0x76, 0xf1, 0x65, 0x29
    }};

    static const bluetoe::details::uint128_t R = {{
        0xc8, 0x0f, 0x2d, 0x0c, 0xd2, 0x42, 0xda, 0x08,
        0x54, 0xbb, 0x53, 0xb4, 0x3b, 0x34, 0xa3, 0x12
    }};

    static const bluetoe::details::io_capabilities_t io_caps = {{
        0x02, 0x01, 0x01
    }};

    const bluetoe::link_layer::public_device_address addr_controller({
        0xce, 0xbf, 0x37, 0x37, 0x12, 0x56
    });

    const bluetoe::link_layer::public_device_address addr_peripheral({
        0xc1, 0xcf, 0x2d, 0x70, 0x13, 0xa7
    });

    static const bluetoe::details::uint128_t expected_check_value = {{
        0x61, 0x8f, 0x95, 0xda, 0x09, 0x0b, 0x6c, 0xd2,
        0xc5, 0xe8, 0xd0, 0x9c, 0x98, 0x73, 0xc4, 0xe3
    }};

    radio_t radio;
    const bluetoe::details::uint128_t check_value = radio.f6(
        mac_key, nonce_central, nonce_periperal, R, io_caps, addr_controller, addr_peripheral );

    return std::equal( check_value.begin(), check_value.end(), expected_check_value.begin() );
}

static bool try_select_random_nonce_test()
{
    std::uint8_t counts[ 256 ] = { 0 };

    radio_t radio;
    const auto nonce = radio.select_random_nonce();

    for ( const std::uint8_t byte: nonce )
    {
        if ( counts[ byte ] != 0 )
            return false;

        ++counts[ byte ];
    }

    return true;
}

static bool select_random_nonce_test()
{
    bool result = try_select_random_nonce_test();

    for ( int i = 0; i != 10 && !result; ++i )
        result = try_select_random_nonce_test();

    return result;
}

static bool g2_test()
{
    // D.5 g2 LE SC NUMERIC COMPARISON GENERATION FUNCTION
    // U              20b003d2 f297be2c 5e2c83a7 e9f9a5b9
    //                eff49111 acf4fddb cc030148 0e359de6
    // V              55188b3d 32f6bb9a 900afcfb eed4e72a
    //                59cb9ac2 f19d7cfb 6b4fdd49 f47fc5fd
    // X              d5cb8454 d177733e ffffb2ec 712baeab
    // Y              a6e8e7cc 25a75f6e 216583f7 ff3dc4cf
    // M0             20b003d2 f297be2c 5e2c83a7 e9f9a5b9
    // M1             eff49111 acf4fddb cc030148 0e359de6
    // M2             55188b3d 32f6bb9a 900afcfb eed4e72a
    // M3             59cb9ac2 f19d7cfb 6b4fdd49 f47fc5fd
    // M4             a6e8e7cc 25a75f6e 216583f7 ff3dc4cf
    // AES_CMAC       1536d18d e3d20df9 9b7044c1 2f9ed5ba
    // g2             2f9ed5ba

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

    const bluetoe::details::uint128_t y = {{
        0xcf, 0xc4, 0x3d, 0xff,
        0xf7, 0x83, 0x65, 0x21,
        0x6e, 0x5f, 0xa7, 0x25,
        0xcc, 0xe7, 0xe8, 0xa6
    }};

    radio_t radio;
    return radio.g2( u.data(), v.data(), x, y ) == 0x2f9ed5bau;
}

// make it easier to set a break point here
extern "C" __attribute__((optimize("O0"))) void test_failed( const char* name )
{
    for ( ;; )
        ;
}

extern "C" __attribute__((optimize("O0"))) void test_success()
{
    for ( ;; )
        ;
}

__attribute__((optimize("O0"))) void check_result( bool result, const char* name )
{
    if ( !result )
        test_failed( name );
}

int main()
{
    // legacy pairing
    check_result( c1_test(), "c1_test" );
    check_result( s1_test(), "s1_test" );
    check_result( setup_encryption_test(), "setup_encryption_test" );

    // lesc pairing
    check_result( is_valid_public_key_test(), "is_valid_public_key_test" );
    check_result( generate_keys_test(), "generate_keys_test" );
    check_result( p256_test(), "p256_test" );
    check_result( f4_test(), "f4_test" );
    check_result( f5_test(), "f5_test" );
    check_result( f6_test(), "f6_test" );
    check_result( select_random_nonce_test(), "select_random_nonce_test" );
    check_result( g2_test(), "g2_test" );

    test_success();
}
