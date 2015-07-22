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

static const std::uint8_t a_few_channels_map[] = { 0x17, 0x44, 0x00, 0xff, 0x05 };

template < unsigned Hop >
struct only_a_few_channels : bluetoe::link_layer::channel_map
{
    only_a_few_channels()
    {
        BOOST_REQUIRE( reset( a_few_channels_map, Hop ) );
    }
};

using only_a_few_channels_7 = only_a_few_channels< 7 >;

/* the list of channels in the map is:
   0, 1, 2, 4, 10, 14, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34
   #Channels = 16
 */
BOOST_FIXTURE_TEST_CASE( only_a_few_channels_hop_7, only_a_few_channels_7 )
{
    // we check the whole sequence
    BOOST_CHECK_EQUAL( data_channel( 0  ), 25 ); // 7 % 16
    BOOST_CHECK_EQUAL( data_channel( 1  ), 14 );
    BOOST_CHECK_EQUAL( data_channel( 2  ), 14 ); // 21 % 16
    BOOST_CHECK_EQUAL( data_channel( 3  ), 28 );
    BOOST_CHECK_EQUAL( data_channel( 4  ), 4  ); // 35 % 16
    BOOST_CHECK_EQUAL( data_channel( 5  ), 14 ); // 5 % 16
    BOOST_CHECK_EQUAL( data_channel( 6  ), 30 ); // 12 % 16
    BOOST_CHECK_EQUAL( data_channel( 7  ), 4  ); // 19 % 16
    BOOST_CHECK_EQUAL( data_channel( 8  ), 26 );
    BOOST_CHECK_EQUAL( data_channel( 9  ), 1  ); // 33 % 16
    BOOST_CHECK_EQUAL( data_channel( 10 ), 4  ); // 3 % 16
    BOOST_CHECK_EQUAL( data_channel( 11 ), 10 );
    BOOST_CHECK_EQUAL( data_channel( 12 ), 1  ); // 17 % 16
    BOOST_CHECK_EQUAL( data_channel( 13 ), 24 );
    BOOST_CHECK_EQUAL( data_channel( 14 ), 31 );
    BOOST_CHECK_EQUAL( data_channel( 15 ), 1  );
    BOOST_CHECK_EQUAL( data_channel( 16 ), 26 ); // 8 % 16
    BOOST_CHECK_EQUAL( data_channel( 17 ), 34 ); // 15 % 16
    BOOST_CHECK_EQUAL( data_channel( 18 ), 24 ); // 22 % 16
    BOOST_CHECK_EQUAL( data_channel( 19 ), 29 );
    BOOST_CHECK_EQUAL( data_channel( 20 ), 10 ); // 36 % 16
    BOOST_CHECK_EQUAL( data_channel( 21 ), 24 ); // 6 % 16
    BOOST_CHECK_EQUAL( data_channel( 22 ), 31 ); // 13 % 16
    BOOST_CHECK_EQUAL( data_channel( 23 ), 10 ); // 20 % 16
    BOOST_CHECK_EQUAL( data_channel( 24 ), 27 );
    BOOST_CHECK_EQUAL( data_channel( 25 ), 34 );
    BOOST_CHECK_EQUAL( data_channel( 26 ), 4  );
    BOOST_CHECK_EQUAL( data_channel( 27 ), 29 ); // 11 % 16
    BOOST_CHECK_EQUAL( data_channel( 28 ), 2  ); // 18 % 16
    BOOST_CHECK_EQUAL( data_channel( 29 ), 25 );
    BOOST_CHECK_EQUAL( data_channel( 30 ), 32 );
    BOOST_CHECK_EQUAL( data_channel( 31 ), 2  );
    BOOST_CHECK_EQUAL( data_channel( 32 ), 27 ); // 9 % 16
    BOOST_CHECK_EQUAL( data_channel( 33 ), 0  ); // 16 % 16
    BOOST_CHECK_EQUAL( data_channel( 34 ), 25 ); // 23 % 16
    BOOST_CHECK_EQUAL( data_channel( 35 ), 30 );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0  );
}

BOOST_FIXTURE_TEST_CASE( invalid_hops_are_recognized, bluetoe::link_layer::channel_map )
{
    BOOST_CHECK( !reset( all_channel_map, 0 ) );
    BOOST_CHECK( !reset( a_few_channels_map, 4 ) );
    BOOST_CHECK( !reset( all_but_one_map, 17 ) );
    BOOST_CHECK( !reset( all_channel_map, 99 ) );
}

BOOST_FIXTURE_TEST_CASE( valid_hops_are_recognized, bluetoe::link_layer::channel_map )
{
    BOOST_CHECK( reset( all_channel_map, 5 ) );
    BOOST_CHECK( reset( a_few_channels_map, 7 ) );
    BOOST_CHECK( reset( all_but_one_map, 10 ) );
    BOOST_CHECK( reset( all_channel_map, 16 ) );
}
