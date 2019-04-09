#include <bluetoe/bindings/nrf51.hpp>
#include <cassert>
#include <algorithm>

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

int main()
{
    assert( c1_test() );
    assert( s1_test() );
    assert( setup_encryption_test() );

    for ( ; ; )
        ;
}
