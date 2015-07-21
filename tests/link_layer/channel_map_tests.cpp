#include <bluetoe/link_layer/channel_map.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

static const std::uint8_t all_channel_map[] = { 0xff, 0xff, 0xff, 0xff, 0x1f };
static const std::uint8_t all_but_one_map[] = { 0xff, 0xff, 0xff, 0xff, 0xf };

template < unsigned Hop >
struct all_channel : bluetoe::link_layer::channel_map
{
    all_channel()
    {
        BOOST_REQUIRE( reset( all_channel_map, Hop ) );
    }
};

template < unsigned Hop >
struct all_but_one_channel : bluetoe::link_layer::channel_map
{
    all_but_one_channel()
    {
        BOOST_REQUIRE( reset( all_but_one_map, Hop ) );
    }
};

using all_channel_5 = all_channel< 5 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_5, all_channel_5 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 5 );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 10 );
    //...
    BOOST_CHECK_EQUAL( data_channel( 17 ), 16 );
    //...
    BOOST_CHECK_EQUAL( data_channel( 35 ), 32 );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0 );
}

using all_channel_16 = all_channel< 16 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_16, all_channel_16 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 16 );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 32 );
    //...
    BOOST_CHECK_EQUAL( data_channel( 11 ), 7 );
    //...
    BOOST_CHECK_EQUAL( data_channel( 35 ), 21 );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0 );
}

using all_channel_10 = all_channel< 10 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_10, all_channel_10 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 10 );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 20 );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 7 ), 6 );
    BOOST_CHECK_EQUAL( data_channel( 10 ), 36 );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 35 ), ( 36 * 10 ) % 37 );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0 );
}

using all_but_one_channel_10 = all_but_one_channel< 10 >;

/*
 * if the last channel (channel 36) is not within the map, it have to be remaped to 36 mod 36 == 0
 */
BOOST_FIXTURE_TEST_CASE( all_but_one_channels_hop_10, all_but_one_channel_10 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 10 );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 20 );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 10 ), 0 );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 35 ), ( 36 * 10 ) % 37 );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0 );
}

BOOST_FIXTURE_TEST_CASE( invalid_hops_are_recognized, bluetoe::link_layer::channel_map )
{
    BOOST_CHECK( !reset( all_channel_map, 0 ) );
    BOOST_CHECK( !reset( all_channel_map, 4 ) );
    BOOST_CHECK( !reset( all_channel_map, 17 ) );
    BOOST_CHECK( !reset( all_channel_map, 99 ) );
}

BOOST_FIXTURE_TEST_CASE( valid_hops_are_recognized, bluetoe::link_layer::channel_map )
{
    BOOST_CHECK( reset( all_channel_map, 5 ) );
    BOOST_CHECK( reset( all_channel_map, 7 ) );
    BOOST_CHECK( reset( all_channel_map, 10 ) );
    BOOST_CHECK( reset( all_channel_map, 16 ) );
}
