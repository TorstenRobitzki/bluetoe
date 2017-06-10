#include <iostream>
#include <cstdint>

#include <bluetoe/characteristic.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/client_characteristic_configuration.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include "hexdump.hpp"
#include "test_attribute_access.hpp"
#include "test_characteristics.hpp"

BOOST_AUTO_TEST_CASE( even_the_simplest_characteristic_has_at_least_2_attributes )
{
    BOOST_CHECK_GE( std::size_t( simple_char::number_of_attributes ), 2 );
}

BOOST_FIXTURE_TEST_CASE( the_first_attribute_is_the_characteristic_declaration, simple_char )
{
    const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );

    BOOST_CHECK_EQUAL( char_declaration.uuid, 0x2803 );
}

BOOST_AUTO_TEST_SUITE( characteristic_declaration_access )

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_has_a_length_of_19, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_REQUIRE( char_declaration.access );
        char_declaration.access( read, 1 );

        BOOST_CHECK_EQUAL( read.buffer_size, 19u );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_has_a_length_of_17_with_offset_2, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 2 );

        BOOST_REQUIRE( char_declaration.access );
        char_declaration.access( read, 1 );

        BOOST_CHECK_EQUAL( read.buffer_size, 17u );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_has_a_length_of_0_with_offset_19, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 19 );

        BOOST_REQUIRE( char_declaration.access );
        BOOST_CHECK( char_declaration.access( read, 1 ) == bluetoe::details::attribute_access_result::success );

        BOOST_CHECK_EQUAL( read.buffer_size, 0 );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_returns_invalid_offset_when_read_behind_data, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 20 );

        BOOST_CHECK( char_declaration.access( read, 1 ) == bluetoe::details::attribute_access_result::invalid_offset );
    }

    BOOST_FIXTURE_TEST_CASE( the_short_characteristic_declaration_has_a_length_of_5, short_uuid_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        char_declaration.access( read, 1 );

        BOOST_CHECK_EQUAL( read.buffer_size, 5u );
    }

    BOOST_FIXTURE_TEST_CASE( the_short_characteristic_declaration_has_a_length_of_4_with_offset_1, short_uuid_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 1 );

        char_declaration.access( read, 1 );

        BOOST_CHECK_EQUAL( read.buffer_size, 4u );
    }

    BOOST_FIXTURE_TEST_CASE( the_short_characteristic_declaration_has_a_length_of_1_with_offset_4, short_uuid_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 4 );

        char_declaration.access( read, 1 );

        BOOST_CHECK_EQUAL( read.buffer_size, 1u );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_uuid, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 1 ) );

        // D0B10674-6DDD-4B59-89CA-A009B78C956B
        static const std::uint8_t expected_uuid[] = { 0x6B, 0x95, 0x8C, 0xB7, 0x09, 0xA0, 0xCA, 0x89, 0x59, 0x4B, 0xDD, 0x6D, 0x74, 0x06, 0xB1, 0xD0 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 3 ], &buffer[ 19 ] );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_uuid_at_offset_3, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 3 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 1 ) );

        // D0B10674-6DDD-4B59-89CA-A009B78C956B
        static const std::uint8_t expected_uuid[] = { 0x6B, 0x95, 0x8C, 0xB7, 0x09, 0xA0, 0xCA, 0x89, 0x59, 0x4B, 0xDD, 0x6D, 0x74, 0x06, 0xB1, 0xD0 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 0 ], &buffer[ 16 ] );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_value_handle, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 0x1234 ) );

        static const std::uint8_t expected_handle[] = { 0x35, 0x12 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_handle ), std::end( expected_handle ), &buffer[ 1 ], &buffer[ 3 ] );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_value_handle_at_offset_1, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 1 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 0x1234 ) );

        static const std::uint8_t expected_handle[] = { 0x35, 0x12 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_handle ), std::end( expected_handle ), &buffer[ 0 ], &buffer[ 2 ] );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_hibyte_of_the_value_handle_at_offset_2, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 2 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 0x1234 ) );

        BOOST_CHECK_EQUAL( buffer[ 0 ], 0x12 );
    }

    BOOST_FIXTURE_TEST_CASE( read_char_declaration_buffer_to_small, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 17 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 1 ) );
        BOOST_CHECK_EQUAL( read.buffer_size, 17u );
        static const std::uint8_t expected_uuid[] = { 0x6B, 0x95, 0x8C, 0xB7, 0x09, 0xA0, 0xCA, 0x89, 0x59, 0x4B, 0xDD, 0x6D, 0x74, 0x06 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 3 ], &buffer[ 17 ] );
    }

    BOOST_FIXTURE_TEST_CASE( read_char_declaration_buffer_to_small_with_offset_1, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 17 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 1 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 1 ) );
        BOOST_CHECK_EQUAL( read.buffer_size, 17u );
        static const std::uint8_t expected_uuid[] = { 0x6B, 0x95, 0x8C, 0xB7, 0x09, 0xA0, 0xCA, 0x89, 0x59, 0x4B, 0xDD, 0x6D, 0x74, 0x06, 0xB1 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 2 ], &buffer[ 17 ] );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_16bit_uuid, short_uuid_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::success == char_declaration.access( read, 1 ) );

        static const std::uint8_t expected_uuid[] = { 0xB1, 0xD0 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 3 ], &buffer[ 5 ] );
    }

    BOOST_FIXTURE_TEST_CASE( char_declaration_is_not_writable, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 4 ];
        auto write = bluetoe::details::attribute_access_arguments::write( buffer );

        BOOST_CHECK( char_declaration.access( write, 1 ) == bluetoe::details::attribute_access_result::write_not_permitted );
    }

    BOOST_FIXTURE_TEST_CASE( read_zero_bytes, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer;

        auto read = bluetoe::details::attribute_access_arguments::read( &buffer, &buffer, 0, bluetoe::details::client_characteristic_configuration(), nullptr );
        auto rc   = char_declaration.access( read, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK( read.buffer_size == 0 );
    }

    BOOST_FIXTURE_TEST_CASE( read_first_single_byte, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer;

        auto read = bluetoe::details::attribute_access_arguments::read( &buffer, &buffer + 1, 0, bluetoe::details::client_characteristic_configuration(), nullptr );
        auto rc   = char_declaration.access( read, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK( read.buffer_size == 1 );
        BOOST_CHECK( buffer == 0x0A ); // property == read + write
    }

    BOOST_FIXTURE_TEST_CASE( read_two_bytes_second_byte_points_into_the_uuid, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 2 ];

        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 2 );
        auto rc   = char_declaration.access( read, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK( read.buffer_size == 2 );
        BOOST_CHECK( buffer[ 0 ] == 0x00 ); // hi byte char value handle
        BOOST_CHECK( buffer[ 1 ] == 0x6B ); // first byte of the uuid
    }

    BOOST_FIXTURE_TEST_CASE( read_one_byte_from_the_uuid, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at< 0 >( 0 );
        std::uint8_t buffer[ 1 ];

        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 4 );
        auto rc   = char_declaration.access( read, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK( read.buffer_size == 1 );
        BOOST_CHECK( buffer[ 0 ] == 0x95 );
    }


BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE( characteristic_properties )

    BOOST_FIXTURE_TEST_CASE( by_default_an_attribute_is_readable_and_writeable, read_characteristic_properties< simple_char > )
    {
        BOOST_CHECK( properties & 0x02 );
        BOOST_CHECK( properties & 0x08 );
    }

    BOOST_FIXTURE_TEST_CASE( read_only, read_characteristic_properties< simple_const_char > )
    {
        BOOST_CHECK( properties & 0x02 );
        BOOST_CHECK_EQUAL( properties & 0x08, 0 );
    }

    typedef bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_write_access
        > simple_char_without_write_access;

    BOOST_FIXTURE_TEST_CASE( write_only, read_characteristic_properties< simple_char_without_write_access > )
    {
        BOOST_CHECK( properties & 0x02 );
        BOOST_CHECK_EQUAL( properties & 0x08, 0 );
    }

    typedef bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_write_access,
            bluetoe::no_read_access
        > simple_char_without_any_access;

    BOOST_FIXTURE_TEST_CASE( only, read_characteristic_properties< simple_char_without_any_access > )
    {
        BOOST_CHECK_EQUAL( properties & 0x02, 0 );
        BOOST_CHECK_EQUAL( properties & 0x08, 0 );
    }

    typedef bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::notify
        > simple_char_with_notification;

    BOOST_FIXTURE_TEST_CASE( with_notify, read_characteristic_properties< simple_char_with_notification > )
    {
        BOOST_CHECK_EQUAL( properties & 0x10, 0x10 );
    }

    BOOST_FIXTURE_TEST_CASE( without_notify, read_characteristic_properties< simple_char > )
    {
        BOOST_CHECK_EQUAL( properties & 0x10, 0 );
    }

    typedef bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::indicate
        > simple_char_with_indication;

    BOOST_FIXTURE_TEST_CASE( with_indicate, read_characteristic_properties< simple_char_with_indication > )
    {
        BOOST_CHECK_EQUAL( properties & 0x20, 0x20 );
    }

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
        bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
        bluetoe::write_without_response
    > write_without_response_char;

    BOOST_FIXTURE_TEST_CASE( with_write_without_response, read_characteristic_properties< write_without_response_char > )
    {
        BOOST_CHECK_EQUAL( properties & 0x04, 0x04 );
    }

    BOOST_FIXTURE_TEST_CASE( without_write_without_response, read_characteristic_properties< simple_char_with_indication > )
    {
        BOOST_CHECK_EQUAL( properties & 0x04, 0x00 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( characteristic_extended_properties )

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( characteristic_user_description )

    char simple_value = 0;
    static constexpr char name[] = "Die ist der Name";

    typedef bluetoe::characteristic<
        bluetoe::characteristic_name< name >,
        bluetoe::characteristic_uuid16< 0x0815 >,
        bluetoe::bind_characteristic_value< char, &simple_value >
    > named_char;

    BOOST_FIXTURE_TEST_CASE( there_is_an_attribute_with_the_given_name, access_attributes< named_char > )
    {
        BOOST_CHECK_EQUAL( 3, int(number_of_attributes) );
        compare_characteristic( { 'D', 'i', 'e', ' ', 'i', 's', 't', ' ', 'd', 'e', 'r', ' ', 'N', 'a', 'm', 'e' }, 0x2901 );
    }

    BOOST_FIXTURE_TEST_CASE( characteristic_user_description_is_readable_with_offset, access_attributes< named_char > )
    {
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 12 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == attribute_by_type( 0x2901 ).access( read, 1 ) );

        static const std::uint8_t expected_value[] = { 'N', 'a', 'm', 'e' };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), read.buffer, read.buffer + read.buffer_size );
    }

    BOOST_FIXTURE_TEST_CASE( characteristic_user_description_is_readable_with_offset_16, access_attributes< named_char > )
    {
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 16 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == attribute_by_type( 0x2901 ).access( read, 1 ) );
        BOOST_CHECK_EQUAL( read.buffer_size, 0 );
    }

    BOOST_FIXTURE_TEST_CASE( characteristic_user_description_is_not_readable_behind, access_attributes< named_char > )
    {
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 17 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::invalid_offset == attribute_by_type( 0x2901 ).access( read, 1 ) );
    }

    BOOST_FIXTURE_TEST_CASE( is_not_writeable, access_attributes< named_char > )
    {
        std::uint8_t buffer[ 100 ];

        auto attr  = attribute_by_type( 0x2901 );
        auto write = bluetoe::details::attribute_access_arguments::write( buffer );

        BOOST_CHECK( attr.access( write, 1 ) == bluetoe::details::attribute_access_result::write_not_permitted );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( client_characteristic_configuration )

    BOOST_FIXTURE_TEST_CASE( does_not_exist_by_default, access_attributes< simple_char > )
    {
        BOOST_CHECK( !find_attribute_by_type( 0x2902 ).first );
    }

    char simple_value = 0;

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x0815 >,
        bluetoe::bind_characteristic_value< char, &simple_value >,
        bluetoe::notify
    > notified_char;

    BOOST_FIXTURE_TEST_CASE( exist_when_notification_is_enabled, access_attributes< notified_char > )
    {
        BOOST_CHECK( find_attribute_by_type( 0x2902 ).first );
    }

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x0815 >,
        bluetoe::bind_characteristic_value< char, &simple_value >,
        bluetoe::notify
    > indicated_char;

    BOOST_FIXTURE_TEST_CASE( exist_when_indication_is_enabled, access_attributes< indicated_char > )
    {
        BOOST_CHECK( find_attribute_by_type( 0x2902 ).first );
    }

    BOOST_FIXTURE_TEST_CASE( has_3_attributes, notified_char )
    {
        BOOST_CHECK_EQUAL( int( number_of_attributes ), 3u );
    }

    BOOST_FIXTURE_TEST_CASE( number_of_client_configs_is_one, notified_char )
    {
        BOOST_CHECK_EQUAL( int( number_of_client_configs ), 1u );
    }

    BOOST_FIXTURE_TEST_CASE( characteristic_can_be_written, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x01, 0x00 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        auto rc    = attribute_by_type( 0x2902 ).access( write, 0 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
    }

    BOOST_FIXTURE_TEST_CASE( characteristic_can_be_written_to_small, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x01 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        auto rc    = attribute_by_type( 0x2902 ).access( write, 0 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );

        compare_characteristic( { 0x01, 0x00 }, 0x2902 );
    }

    BOOST_FIXTURE_TEST_CASE( characteristic_cant_be_written_to_large, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x01, 0x02, 0x03 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        auto rc    = attribute_by_type( 0x2902 ).access( write, 0 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::invalid_attribute_value_length );
    }

    BOOST_FIXTURE_TEST_CASE( and_be_read, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x03, 0x00 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::success );

        compare_characteristic( { 0x03, 0x00 }, 0x2902 );
    }

    BOOST_FIXTURE_TEST_CASE( and_be_read_with_offset, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x03, 0x00 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::success );

        std::uint8_t buffer[ 10 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 1, client_configurations() );
        auto rc   = attribute_by_type( 0x2902 ).access( read, 0 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK_EQUAL( read.buffer_size, 1 );
        BOOST_CHECK_EQUAL( read.buffer[ 0 ], 0 );
    }

    BOOST_FIXTURE_TEST_CASE( and_be_read_with_offset_equal_size, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x03, 0x00 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::success );

        std::uint8_t buffer[ 10 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 2, client_configurations() );
        auto rc   = attribute_by_type( 0x2902 ).access( read, 0 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK_EQUAL( read.buffer_size, 0 );
    }

    BOOST_FIXTURE_TEST_CASE( and_be_read_with_offset_out_of_range, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0x03, 0x00 };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 0, client_configurations() );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::success );

        std::uint8_t buffer[ 10 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 3, client_configurations() );
        auto rc   = attribute_by_type( 0x2902 ).access( read, 0 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::invalid_offset );
    }

    BOOST_FIXTURE_TEST_CASE( write_only_the_last_byte, access_attributes< notified_char > )
    {
        static const std::uint8_t bytes_to_write[] = { 0xff };

        auto write = bluetoe::details::attribute_access_arguments::write( bytes_to_write, 1, client_configurations() );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::success );

        // write will be ignored
        compare_characteristic( { 0x00, 0x00 }, 0x2902 );
    }

    BOOST_FIXTURE_TEST_CASE( write_zero_bytes_at_the_end, access_attributes< notified_char > )
    {
        static const std::uint8_t byte = 0;

        auto write = bluetoe::details::attribute_access_arguments::write( &byte, &byte, 2, client_configurations(), nullptr );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::success );

        // write as no effect
        compare_characteristic( { 0x00, 0x00 }, 0x2902 );
    }

    BOOST_FIXTURE_TEST_CASE( write_over_the_end, access_attributes< notified_char > )
    {
        static const std::uint8_t byte = 0;

        auto write = bluetoe::details::attribute_access_arguments::write( &byte, &byte + 1, 2, client_configurations(), nullptr );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::invalid_attribute_value_length );
    }

    BOOST_FIXTURE_TEST_CASE( write_behind_the_end, access_attributes< notified_char > )
    {
        static const std::uint8_t byte = 0;

        auto write = bluetoe::details::attribute_access_arguments::write( &byte, &byte + 1, 3, client_configurations(), nullptr );
        BOOST_REQUIRE( attribute_by_type( 0x2902 ).access( write, 0 ) == bluetoe::details::attribute_access_result::invalid_offset );
    }


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( server_characteristic_configuration )

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( characteristic_presentation_format )

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( characteristic_aggregate_format )

BOOST_AUTO_TEST_SUITE_END()
