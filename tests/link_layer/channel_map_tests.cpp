#include <bluetoe/link_layer/channel_map.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

static const std::uint8_t all_channel_map[] = { 0xff, 0xff, 0xff, 0xff, 0x1f };

template < unsigned Hop >
struct all_channel : bluetoe::link_layer::channel_map
{
    all_channel()
    {
        reset( all_channel_map, Hop );
    }
};

using all_channel_1 = all_channel< 1 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_1, all_channel_1 )
{
    BOOST_CHECK_EQUAL( next_channel( 0 ), 1 );
    BOOST_CHECK_EQUAL( next_channel( 7 ), 8 );
    BOOST_CHECK_EQUAL( next_channel( 35 ), 36 );
    BOOST_CHECK_EQUAL( next_channel( 36 ), 0 );
}

using all_channel_3 = all_channel< 3 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_3, all_channel_3 )
{
    BOOST_CHECK_EQUAL( next_channel( 0 ), 3 );
    BOOST_CHECK_EQUAL( next_channel( 7 ), 10 );
    BOOST_CHECK_EQUAL( next_channel( 35 ), 1 );
    BOOST_CHECK_EQUAL( next_channel( 36 ), 2 );
}