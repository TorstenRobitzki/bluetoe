#include <iostream>
#include "test_services.hpp"
#include "hexdump.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <iterator>

BOOST_AUTO_TEST_CASE( service_without_any_characteristic_results_in_one_attribute )
{
    const auto number = test::empty_service::number_of_attributes;
    BOOST_CHECK_EQUAL( 1u, number );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );
    BOOST_CHECK_EQUAL( 0x2800, attr.uuid );
}

static const std::uint8_t global_temperature_service_uuid[ 16 ] = { 0x2A, 0xD9, 0x91, 0x11, 0xAB, 0x5B, 0x58, 0xB0, 0x3B, 0x4F, 0x50, 0x44, 0x52, 0x6E, 0x42, 0xF0 };

static void check_service_uuid( const bluetoe::details::attribute_access_arguments& args, std::size_t size = sizeof( global_temperature_service_uuid ) )
{
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( global_temperature_service_uuid ), std::begin( global_temperature_service_uuid ) + size, args.buffer, args.buffer + args.buffer_size );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );
    BOOST_REQUIRE( attr.access );

    std::uint8_t buffer[ 16 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    const auto access_result = attr.access( read, 1 );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    check_service_uuid( read );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read_buffer_larger )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );

    std::uint8_t buffer[ 20 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    const auto access_result = attr.access( read, 1 );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    check_service_uuid( read );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read_buffer_larger_with_offset )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );

    std::uint8_t buffer[ 20 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 4 );
    const auto access_result = attr.access( read, 1 );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( global_temperature_service_uuid ) + 4, std::end( global_temperature_service_uuid ),
        &read.buffer[ 0 ], &read.buffer[ read.buffer_size ]);
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read_buffer_larger_with_offset_16 )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );

    std::uint8_t buffer[ 20 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 16 );
    const auto access_result = attr.access( read, 1 );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    BOOST_CHECK_EQUAL( 0, read.buffer_size );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read_buffer_to_small )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );

    std::uint8_t buffer[ 15 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    const auto access_result = attr.access( read, 1 );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    check_service_uuid( read, sizeof( buffer ) );
}

BOOST_AUTO_TEST_CASE( write_to_primary_service )
{
    const auto attr = test::empty_service::attribute_at< 0, std::tuple< test::empty_service > >( 0 );

    std::uint8_t buffer[] = { 1, 2, 3 };
    auto write = bluetoe::details::attribute_access_arguments::write( buffer );
    const auto access_result = attr.access( write, 1 );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::write_not_permitted );
}

BOOST_FIXTURE_TEST_CASE( accessing_all_attributes, test::service_with_3_characteristics )
{
    static constexpr std::size_t expected_number_of_attributes = 7u;
    BOOST_REQUIRE_EQUAL( unsigned( number_of_attributes ), expected_number_of_attributes );

    BOOST_CHECK_EQUAL( 0x2800, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 0 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x2803, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 1 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x0001, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 2 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x2803, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 3 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x0001, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 4 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x2803, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 5 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x0815, ( attribute_at< 0, std::tuple< test::service_with_3_characteristics > >( 6 ).uuid ) );
}

typedef std::tuple< test::service_with_3_characteristics > service_with_3_characteristics_list;

BOOST_FIXTURE_TEST_CASE( read_by_group_type_response, test::service_with_3_characteristics )
{
    std::uint8_t    buffer[ 100 ];

    std::uint8_t* const end = read_primary_service_response< 0, service_with_3_characteristics_list >( std::begin( buffer ), std::end( buffer ), 12, true, *this );

    BOOST_CHECK_EQUAL( end - std::begin( buffer ), 20u );

    static const std::uint8_t expected_result[] =
    {
        0x0c, 0x00,              // Starting Handle
        0x12, 0x00,              // Ending Handle
        0xA9, 0x3C, 0xC7, 0x5B,  // Attribute Value == 128 bit UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), end, std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_FIXTURE_TEST_CASE( read_by_group_type_response_buffer_to_small, test::service_with_3_characteristics )
{
    std::uint8_t    buffer[ 19 ];
    std::uint16_t   index = 1;

    std::uint8_t* const end = read_primary_service_response< 0, service_with_3_characteristics_list >( std::begin( buffer ), std::end( buffer ), index, true, *this );

    BOOST_CHECK( end == std::begin( buffer ) );
}

typedef std::tuple< test::cycling_speed_and_cadence_service > cycling_speed_and_cadence_service_list;

BOOST_FIXTURE_TEST_CASE( read_by_group_type_response_for_16bit_uuid, test::cycling_speed_and_cadence_service )
{
    std::uint8_t    buffer[ 100 ];

    std::uint8_t* const end = read_primary_service_response< 0, cycling_speed_and_cadence_service_list >( std::begin( buffer ), std::end( buffer ), 1, false, *this );

    BOOST_CHECK_EQUAL( end - std::begin( buffer ), 6u );

    static const std::uint8_t expected_result[] =
    {
        0x01, 0x00,   // Starting Handle
        0x05, 0x00,   // Ending Handle
        0x16, 0x18    // Attribute Value == 16 bit UUID
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), end, std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_FIXTURE_TEST_CASE( primary_service_value_compare_128bit, test::global_temperature_service )
{
    auto compare = bluetoe::details::attribute_access_arguments::compare_value( &global_temperature_service_uuid[ 0 ], &global_temperature_service_uuid[ sizeof global_temperature_service_uuid ], nullptr );
    BOOST_CHECK( ( attribute_at< 0, std::tuple< test::global_temperature_service > >( 0 ).access( compare, 1 ) == bluetoe::details::attribute_access_result::value_equal ) );
}

BOOST_AUTO_TEST_SUITE( number_of_client_configs )

BOOST_AUTO_TEST_CASE( without_notifications_there_is_no_demand_for_client_configurations )
{
    BOOST_CHECK_EQUAL( 0, int( test::service_with_3_characteristics::number_of_client_configs ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( find_notification_data )

char v1, v2, v3;

typedef bluetoe::service<
    bluetoe::service_uuid16< 0x8C8B >,
    bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x8C8B >,
        bluetoe::bind_characteristic_value< decltype( v1 ), &v1 >,
        bluetoe::notify
    >,
    bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x8C8C >,
        bluetoe::bind_characteristic_value< decltype( v2 ), &v2 >
    >,
    bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x8C8D >,
        bluetoe::bind_characteristic_value< decltype( v3 ), &v3 >,
        bluetoe::notify
    >
> service_with_2_notifications;

BOOST_AUTO_TEST_CASE( service_with_2_notifications_has_2_client_configurations )
{
    BOOST_CHECK_EQUAL( 2, int( service_with_2_notifications::number_of_client_configs ) );
}

// just to be sure
BOOST_FIXTURE_TEST_CASE( check_service_with_2_notifications_attribute_layout, service_with_2_notifications )
{
    BOOST_CHECK_EQUAL( 0x2800, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 0 ).uuid ) );

    BOOST_CHECK_EQUAL( 0x2803, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 1 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x8C8B, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 2 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x2902, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 3 ).uuid ) );

    BOOST_CHECK_EQUAL( 0x2803, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 4 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x8C8C, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 5 ).uuid ) );

    BOOST_CHECK_EQUAL( 0x2803, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 6 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x8C8D, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 7 ).uuid ) );
    BOOST_CHECK_EQUAL( 0x2902, ( attribute_at< 0, std::tuple< service_with_2_notifications > >( 8 ).uuid ) );
}

BOOST_AUTO_TEST_SUITE_END()

std::int32_t temperature;

using sensor_position_uuid = bluetoe::service_uuid< 0xD9473E00, 0xE7D3, 0x4D90, 0x9366, 0x282AC4F44FEB >;
using pizza_service_uuid   = bluetoe::service_uuid16< 0x3523 >;

typedef bluetoe::service<
    bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
    bluetoe::include_service< sensor_position_uuid >,
    bluetoe::characteristic<
        bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
        bluetoe::no_read_access
    >
> temperature_service;

typedef bluetoe::secondary_service<
    sensor_position_uuid,
    bluetoe::characteristic<
        bluetoe::fixed_uint8_value< 0x42 >
    >
> sensor_position_service;

typedef std::tuple< temperature_service, sensor_position_service > temperature_and_sensor_position_service;
typedef std::tuple< sensor_position_service, temperature_service > sensor_position_and_temperature_service;

BOOST_AUTO_TEST_SUITE( secondary_service )

BOOST_FIXTURE_TEST_CASE( find_secondary_service_definition, sensor_position_service )
{
    BOOST_CHECK_EQUAL( 0x2801, ( attribute_at< 0, temperature_and_sensor_position_service >( 0 ).uuid ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( include_service )

BOOST_FIXTURE_TEST_CASE( correct_number_of_attributes, temperature_service )
{
    BOOST_CHECK_EQUAL( int( number_of_attributes ), 4 );
    BOOST_CHECK_EQUAL( int( number_of_service_attributes ), 2 );
}

BOOST_FIXTURE_TEST_CASE( find_include_definition_in_first_service, temperature_service )
{
    BOOST_CHECK_EQUAL( 0x2802, ( attribute_at< 0, sensor_position_and_temperature_service >( 1 ).uuid ) );
}

BOOST_FIXTURE_TEST_CASE( find_include_definition_in_second_service, temperature_service )
{
    BOOST_CHECK_EQUAL( 0x2802, ( attribute_at< 0, temperature_and_sensor_position_service >( 1 ).uuid ) );
}

BOOST_FIXTURE_TEST_CASE( read_include_definition_from_second_service_128_bit_uuid, temperature_service )
{
    std::uint8_t buffer[ 10 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    const auto access_result = attribute_at< 0, temperature_and_sensor_position_service >( 1 ).access( read, 1 );

    static const auto first_service_size = temperature_service::number_of_attributes;
    static const auto second_service_size = sensor_position_service::number_of_attributes;

    static const std::uint16_t service_attribute_handle = 1 + first_service_size;
    static const std::uint16_t end_group_handle         = service_attribute_handle + second_service_size - 1;

    static const std::uint8_t expected_result[] = {
        service_attribute_handle & 0xff,
        service_attribute_handle >> 8,
        end_group_handle & 0xff,
        end_group_handle >> 8
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + read.buffer_size,
        std::begin( expected_result ), std::end( expected_result ) );
    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
}

BOOST_FIXTURE_TEST_CASE( read_include_definition_from_first_service_128_bit_uuid, temperature_service )
{
    std::uint8_t buffer[ 10 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    const auto access_result = attribute_at< 0, sensor_position_and_temperature_service >( 1 ).access( read, 1 );

    static const auto first_service_size = sensor_position_service::number_of_attributes;

    static const std::uint16_t service_attribute_handle = 1;
    static const std::uint16_t end_group_handle         = first_service_size;

    static const std::uint8_t expected_result[] = {
        service_attribute_handle & 0xff,
        service_attribute_handle >> 8,
        end_group_handle & 0xff,
        end_group_handle >> 8
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + read.buffer_size,
        std::begin( expected_result ), std::end( expected_result ) );
    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( include_service_16_bit )

typedef bluetoe::service_uuid16< 0x1234 > pizza_service_uuid;
typedef bluetoe::service_uuid16< 0xabcd > beverages_service_uuid;

using pizza_service = bluetoe::service<
    pizza_service_uuid,
    bluetoe::include_service< beverages_service_uuid >,
    bluetoe::characteristic<
        bluetoe::fixed_uint8_value< 0x42 >
    >
>;

using beverages_service = bluetoe::service<
    beverages_service_uuid,
    bluetoe::characteristic<
        bluetoe::fixed_uint8_value< 0x42 >
    >
>;

using pizza_and_drinks_service = std::tuple< pizza_service, beverages_service >;

BOOST_FIXTURE_TEST_CASE( find_include_definition, pizza_service )
{
    BOOST_CHECK_EQUAL( 0x2802, ( attribute_at< 0, pizza_and_drinks_service >( 1 ).uuid ) );
}

BOOST_FIXTURE_TEST_CASE( read_include_definition_from_service, pizza_service )
{
    std::uint8_t buffer[ 10 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    const auto access_result = attribute_at< 0, pizza_and_drinks_service >( 1 ).access( read, 1 );

    static const auto first_service_size  = pizza_service::number_of_attributes;
    static const auto second_service_size = beverages_service::number_of_attributes;

    static const std::uint16_t service_attribute_handle = 1 + first_service_size;
    static const std::uint16_t end_group_handle         = service_attribute_handle + second_service_size - 1;

    static const std::uint8_t expected_result[] = {
        service_attribute_handle & 0xff,
        service_attribute_handle >> 8,
        end_group_handle & 0xff,
        end_group_handle >> 8,
        0xcd, 0xab
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + read.buffer_size,
        std::begin( expected_result ), std::end( expected_result ) );
    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
}


BOOST_AUTO_TEST_SUITE_END()
