#include <bluetoe/link_layer/channel_map.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

static constexpr std::uint8_t all_channel_map[] = { 0xff, 0xff, 0xff, 0xff, 0x1f };
static constexpr std::uint8_t all_but_one_map[] = { 0xff, 0xff, 0xff, 0xff, 0xf };

template < unsigned Hop, const std::uint8_t*  Map >
struct fixture : bluetoe::link_layer::channel_map
{
    fixture()
    {
        BOOST_REQUIRE( reset( Map, Hop ) );
    }
};


template < unsigned Hop >
using all_channel = fixture< Hop, all_channel_map >;

template < unsigned Hop >
using all_but_one_channel = fixture< Hop, all_but_one_map >;

using all_channel_5 = all_channel< 5 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_5, all_channel_5 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 5u );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 10u );
    //...
    BOOST_CHECK_EQUAL( data_channel( 17 ), 16u );
    //...
    BOOST_CHECK_EQUAL( data_channel( 35 ), 32u );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0u );
}

using all_channel_16 = all_channel< 16 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_16, all_channel_16 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 16u );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 32u );
    //...
    BOOST_CHECK_EQUAL( data_channel( 11 ), 7u );
    //...
    BOOST_CHECK_EQUAL( data_channel( 35 ), 21u );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0u );
}

using all_channel_10 = all_channel< 10 >;

BOOST_FIXTURE_TEST_CASE( all_channels_hop_10, all_channel_10 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 10u );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 20u );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 7 ), 6u );
    BOOST_CHECK_EQUAL( data_channel( 10 ), 36u );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 35 ), unsigned{ ( 36 * 10 ) % 37 } );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0u );
}

using all_but_one_channel_10 = all_but_one_channel< 10 >;

/*
 * if the last channel (channel 36) is not within the map, it have to be remaped to 36 mod 36 == 0
 */
BOOST_FIXTURE_TEST_CASE( all_but_one_channels_hop_10, all_but_one_channel_10 )
{
    BOOST_CHECK_EQUAL( data_channel( 0 ), 10u );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 20u );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 10 ), 0u );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 35 ), unsigned{ ( 36 * 10 ) % 37 } );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0u );
}

static constexpr std::uint8_t a_few_channels_map[] = { 0x17, 0x44, 0x00, 0xff, 0x05 };

using only_a_few_channels_7 = fixture< 7, a_few_channels_map >;

/* the list of channels in the map is:
   0, 1, 2, 4, 10, 14, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34
   #Channels = 16
 */
BOOST_FIXTURE_TEST_CASE( only_a_few_channels_hop_7, only_a_few_channels_7 )
{
    // we check the whole sequence
    BOOST_CHECK_EQUAL( data_channel( 0  ), 25u ); // 7 % 16
    BOOST_CHECK_EQUAL( data_channel( 1  ), 14u );
    BOOST_CHECK_EQUAL( data_channel( 2  ), 14u ); // 21 % 16
    BOOST_CHECK_EQUAL( data_channel( 3  ), 28u );
    BOOST_CHECK_EQUAL( data_channel( 4  ), 4u  ); // 35 % 16
    BOOST_CHECK_EQUAL( data_channel( 5  ), 14u ); // 5 % 16
    BOOST_CHECK_EQUAL( data_channel( 6  ), 30u ); // 12 % 16
    BOOST_CHECK_EQUAL( data_channel( 7  ), 4u  ); // 19 % 16
    BOOST_CHECK_EQUAL( data_channel( 8  ), 26u );
    BOOST_CHECK_EQUAL( data_channel( 9  ), 1u  ); // 33 % 16
    BOOST_CHECK_EQUAL( data_channel( 10 ), 4u  ); // 3 % 16
    BOOST_CHECK_EQUAL( data_channel( 11 ), 10u );
    BOOST_CHECK_EQUAL( data_channel( 12 ), 1u  ); // 17 % 16
    BOOST_CHECK_EQUAL( data_channel( 13 ), 24u );
    BOOST_CHECK_EQUAL( data_channel( 14 ), 31u );
    BOOST_CHECK_EQUAL( data_channel( 15 ), 1u  );
    BOOST_CHECK_EQUAL( data_channel( 16 ), 26u ); // 8 % 16
    BOOST_CHECK_EQUAL( data_channel( 17 ), 34u ); // 15 % 16
    BOOST_CHECK_EQUAL( data_channel( 18 ), 24u ); // 22 % 16
    BOOST_CHECK_EQUAL( data_channel( 19 ), 29u );
    BOOST_CHECK_EQUAL( data_channel( 20 ), 10u ); // 36 % 16
    BOOST_CHECK_EQUAL( data_channel( 21 ), 24u ); // 6 % 16
    BOOST_CHECK_EQUAL( data_channel( 22 ), 31u ); // 13 % 16
    BOOST_CHECK_EQUAL( data_channel( 23 ), 10u ); // 20 % 16
    BOOST_CHECK_EQUAL( data_channel( 24 ), 27u );
    BOOST_CHECK_EQUAL( data_channel( 25 ), 34u );
    BOOST_CHECK_EQUAL( data_channel( 26 ), 4u  );
    BOOST_CHECK_EQUAL( data_channel( 27 ), 29u ); // 11 % 16
    BOOST_CHECK_EQUAL( data_channel( 28 ), 2u  ); // 18 % 16
    BOOST_CHECK_EQUAL( data_channel( 29 ), 25u );
    BOOST_CHECK_EQUAL( data_channel( 30 ), 32u );
    BOOST_CHECK_EQUAL( data_channel( 31 ), 2u  );
    BOOST_CHECK_EQUAL( data_channel( 32 ), 27u ); // 9 % 16
    BOOST_CHECK_EQUAL( data_channel( 33 ), 0u  ); // 16 % 16
    BOOST_CHECK_EQUAL( data_channel( 34 ), 25u ); // 23 % 16
    BOOST_CHECK_EQUAL( data_channel( 35 ), 30u );
    BOOST_CHECK_EQUAL( data_channel( 36 ), 0u  );
}

/*
 * A map just containing channel 0 and channel 36
 */
static constexpr std::uint8_t only_two_channels_map[] = { 0x01, 0x00, 0x00, 0x00, 0x10 };

using only_two_channels_8 = fixture< 8, only_two_channels_map >;

BOOST_FIXTURE_TEST_CASE( having_odd_hop_increment_with_two_channels_leeds_to_long_periods_on_the_same_channel, only_two_channels_8 )
{
    BOOST_CHECK_EQUAL( data_channel( 0  ), 0u );
    BOOST_CHECK_EQUAL( data_channel( 1  ), 0u );
    BOOST_CHECK_EQUAL( data_channel( 2  ), 0u );
    // ...
    BOOST_CHECK_EQUAL( data_channel( 34  ), 36u );
    BOOST_CHECK_EQUAL( data_channel( 35  ), 36u );
    BOOST_CHECK_EQUAL( data_channel( 36  ), 0u );
}

/*
 * A map just containing one channel
 */
static const std::uint8_t only_one_channel_map[] = { 0x00, 0x04, 0x00, 0x00, 0x00 };
static const std::uint8_t only_one_channel_and_some_rfu_bits_map[] = { 0x00, 0x04, 0x00, 0x00, 0x60 };

using only_two_channels_8 = fixture< 8, only_two_channels_map >;

// 4.5.8.1 The minimum number of used channels shall be 2.
BOOST_FIXTURE_TEST_CASE( channel_map_shall_contain_at_least_two_bits, bluetoe::link_layer::channel_map )
{
    BOOST_CHECK( !reset( only_one_channel_map, 5u ) );
    BOOST_CHECK( !reset( only_one_channel_map, 11u ) );
    BOOST_CHECK( !reset( only_one_channel_and_some_rfu_bits_map, 5u ) );
    BOOST_CHECK( !reset( only_one_channel_and_some_rfu_bits_map, 11u ) );
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

BOOST_FIXTURE_TEST_CASE( stores_the_right_hop, all_channel_5 )
{
    BOOST_CHECK( reset( only_two_channels_map ) );

    BOOST_CHECK_EQUAL( data_channel( 0 ), 36u );
    BOOST_CHECK_EQUAL( data_channel( 1 ), 0u );
}

static constexpr std::uint8_t all_but_25_map[] = { 0xff, 0xff, 0xff, 0xfd, 0x1f };

using all_channels_but_25 = all_channel< 6 >;

BOOST_FIXTURE_TEST_CASE( real_life_example, all_channels_but_25 )
{
    BOOST_CHECK( reset( all_but_25_map ) );
    BOOST_CHECK_EQUAL( data_channel( 28 ), 26u );
    BOOST_CHECK_EQUAL( data_channel( 29 ), 32u );
    BOOST_CHECK_EQUAL( data_channel( 30 ), 1u );
    BOOST_CHECK_EQUAL( data_channel( 35 ), 31u );
}
