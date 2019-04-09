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

int main()
{
    assert( c1_test() );
}
