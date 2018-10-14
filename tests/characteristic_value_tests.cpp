#include <characteristic.hpp>
#include <service.hpp>
#include <bluetoe/client_characteristic_configuration.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include "hexdump.hpp"
#include "test_attribute_access.hpp"
#include "test_characteristics.hpp"

using cccd_indices = std::tuple<>;
using suuid = bluetoe::service_uuid16< 0x4711 >;
struct srv;

BOOST_AUTO_TEST_SUITE( characteristic_value_access )

    BOOST_FIXTURE_TEST_CASE( simple_value_can_be_read, simple_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( read, 1 ) );

        static const std::uint8_t expected_value[] = { 0xdd, 0xcc, 0xbb, 0xaa };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), read.buffer, read.buffer + read.buffer_size );
    }

    BOOST_FIXTURE_TEST_CASE( char_value_returns_invalid_offset_when_read_behind_the_data, simple_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 5 );

        BOOST_CHECK( bluetoe::details::attribute_access_result::invalid_offset == value_attribute.access( read, 1 ) );
    }

    BOOST_FIXTURE_TEST_CASE( simple_value_can_be_read_with_offset, simple_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 2 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( read, 1 ) );

        static const std::uint8_t expected_value[] = { 0xbb, 0xaa };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), read.buffer, read.buffer + read.buffer_size );
    }

    BOOST_FIXTURE_TEST_CASE( simple_value_can_be_read_with_offset_equal_to_length, simple_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 4 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( read, 1 ) );
        BOOST_CHECK_EQUAL( read.buffer_size, 0u );
    }

    BOOST_FIXTURE_TEST_CASE( simple_value_can_be_written, simple_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t new_value[] = { 0x01, 0x02, 0x03, 0x04 };

        auto write = bluetoe::details::attribute_access_arguments::write( new_value );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( write, 1 ) );

        std::uint8_t buffer[ 4 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( read, 1 ) );

        static const std::uint8_t expected_value[] = { 0x01, 0x02, 0x03, 0x04 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), read.buffer, read.buffer + read.buffer_size );
    }

    BOOST_FIXTURE_TEST_CASE( simple_const_value_can_be_read, simple_const_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( read, 1 ) );

        static const std::uint8_t expected_value[] = { 0xdd, 0xcc, 0xbb, 0xaa };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), read.buffer, read.buffer + read.buffer_size );
    }

    BOOST_FIXTURE_TEST_CASE( simple_const_value_can_not_be_writte, simple_const_char )
    {
        const bluetoe::details::attribute value_attribute = attribute_at< cccd_indices, 0, suuid, srv >( 1 );
        std::uint8_t new_value[] = { 0x01, 0x02, 0x03, 0x04 };

        auto write = bluetoe::details::attribute_access_arguments::write( new_value );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::write_not_permitted == value_attribute.access( write, 1 ) );
    }

    BOOST_AUTO_TEST_CASE( simple_value_without_read_access_can_not_be_read )
    {
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_read_access
        > simple_char_without_write_access;

        const bluetoe::details::attribute value_attribute = simple_char_without_write_access.attribute_at< cccd_indices, 0, suuid, srv >( 1 );

        std::uint8_t old_value[ 4 ];

        auto read = bluetoe::details::attribute_access_arguments::read( old_value, 0 );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::read_not_permitted == value_attribute.access( read, 1 ) );
    }

    BOOST_AUTO_TEST_CASE( simple_value_without_write_access_can_not_be_written )
    {
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_write_access
        > simple_char_without_read_access;

        const bluetoe::details::attribute value_attribute = simple_char_without_read_access.attribute_at< cccd_indices, 0, suuid, srv >( 1 );

        std::uint8_t new_value[] = { 0x01, 0x02, 0x03, 0x04 };

        auto write = bluetoe::details::attribute_access_arguments::write( new_value );

        BOOST_REQUIRE( bluetoe::details::attribute_access_result::write_not_permitted == value_attribute.access( write, 1 ) );
    }

    static std::uint32_t write_to_large_value = 15;

    BOOST_AUTO_TEST_CASE( write_to_large )
    {
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &write_to_large_value >
        > characteristic;

        std::uint8_t new_value[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

        auto write = bluetoe::details::attribute_access_arguments::write( new_value );

        BOOST_REQUIRE( ( bluetoe::details::attribute_access_result::invalid_attribute_value_length == characteristic.attribute_at< cccd_indices, 0, suuid, srv >( 1 ).access( write, 1 ) ) );
        BOOST_CHECK_EQUAL( write_to_large_value, 15u );
    }

    std::uint8_t writable_value[ 4 ] = { 0x01, 0x02, 0x03, 0x04 };

    struct writable_value_char :
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< decltype( writable_value ), &writable_value >
        >
    {
        writable_value_char()
        {
            static const std::uint8_t init_value[ 4 ] = { 0x01, 0x02, 0x03, 0x04 };
            std::copy( std::begin( init_value ), std::end( init_value ), std::begin( writable_value ) );
        }
    };

    BOOST_FIXTURE_TEST_CASE( write_with_offset, writable_value_char )
    {
        static const std::uint8_t new_value[] = { 0x22, 0x33 };
        auto write = bluetoe::details::attribute_access_arguments::write( new_value, 1 );
        auto rc    = attribute_at< cccd_indices, 0, suuid, srv >( 1 ).access( write, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        static const std::uint8_t expected_value[] = { 0x01, 0x22, 0x33, 0x04 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( writable_value ), std::end( writable_value ), std::begin( expected_value ), std::end( expected_value ) );
    }

    BOOST_FIXTURE_TEST_CASE( write_over_end_with_offset, writable_value_char )
    {
        static const std::uint8_t new_value[] = { 0x22, 0x33 };
        auto write = bluetoe::details::attribute_access_arguments::write( new_value, 3 );
        auto rc    = attribute_at< cccd_indices, 0, suuid, srv >( 1 ).access( write, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::invalid_attribute_value_length );
        static const std::uint8_t expected_value[] = { 0x01, 0x02, 0x03, 0x04 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( writable_value ), std::end( writable_value ), std::begin( expected_value ), std::end( expected_value ) );
    }

    BOOST_FIXTURE_TEST_CASE( write_behind_the_end, writable_value_char )
    {
        static const std::uint8_t new_value[] = { 0x22, 0x33 };
        auto write = bluetoe::details::attribute_access_arguments::write( new_value, 5 );
        auto rc    = attribute_at< cccd_indices, 0, suuid, srv >( 1 ).access( write, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::invalid_offset );
        static const std::uint8_t expected_value[] = { 0x01, 0x02, 0x03, 0x04 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( writable_value ), std::end( writable_value ), std::begin( expected_value ), std::end( expected_value ) );
    }

    BOOST_FIXTURE_TEST_CASE( can_write_zero_bytes_at_the_end, writable_value_char )
    {
        std::uint8_t c;
        auto write = bluetoe::details::attribute_access_arguments::write( &c, &c, 4, bluetoe::details::client_characteristic_configuration(), nullptr );
        auto rc    = attribute_at< cccd_indices, 0, suuid, srv >( 1 ).access( write, 1 );
        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
    }

    BOOST_FIXTURE_TEST_CASE( can_write_last_byte, writable_value_char )
    {
        std::uint8_t c = 0xff;
        auto write = bluetoe::details::attribute_access_arguments::write( &c, &c + 1, 3, bluetoe::details::client_characteristic_configuration(), nullptr );
        auto rc    = attribute_at< cccd_indices, 0, suuid, srv >( 1 ).access( write, 1 );

        BOOST_CHECK( rc == bluetoe::details::attribute_access_result::success );
        static const std::uint8_t expected_value[] = { 0x01, 0x02, 0x03, 0xff };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( writable_value ), std::end( writable_value ), std::begin( expected_value ), std::end( expected_value ) );
    }

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE( fixed_value_tests )

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0xD0B1 >,
        bluetoe::fixed_value< std::uint16_t, 0x0815 >
    > fixed_16bit;

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0xD0B1 >,
        bluetoe::fixed_value< std::int32_t, 0x08151213 >
    > fixed_32bit;

    BOOST_FIXTURE_TEST_CASE( find_char, access_attributes< fixed_16bit > )
    {
        attribute_by_type( 0xD0B1 );
    }

    BOOST_FIXTURE_TEST_CASE( properties_are_read_only, read_characteristic_properties< fixed_16bit > )
    {
        BOOST_CHECK_EQUAL( properties, 0x02 );
    }

    BOOST_FIXTURE_TEST_CASE( read_value, access_attributes< fixed_16bit > )
    {
        compare_characteristic( { 0x15, 0x08 }, 0xd0b1 );
    }

    BOOST_FIXTURE_TEST_CASE( read_only, access_attributes< fixed_32bit > )
    {
        const auto attr = attribute_by_type( 0xD0B1 );
        std::uint8_t buffer[ 4 ];
        auto write = bluetoe::details::attribute_access_arguments::write( buffer );

        BOOST_CHECK( attr.access( write, 1 ) == bluetoe::details::attribute_access_result::write_not_permitted );
    }

    BOOST_FIXTURE_TEST_CASE( read_with_offset, access_attributes< fixed_32bit > )
    {
        BOOST_CHECK( read_characteristic_at( { 0x12, 0x15, 0x08 }, 1, 1, 3 )
            == bluetoe::details::attribute_access_result::success );
    }

    BOOST_FIXTURE_TEST_CASE( read_truncated_with_offset, access_attributes< fixed_32bit > )
    {
        BOOST_CHECK( read_characteristic_at( { 0x12, 0x15 }, 1, 1, 2 )
            == bluetoe::details::attribute_access_result::success );
    }

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0xD0B1 >,
        bluetoe::fixed_value< std::int32_t, 0x08151213 >,
        bluetoe::no_read_access
    > fixed_without_read_access;

    BOOST_FIXTURE_TEST_CASE( read_without_read_access, access_attributes< fixed_without_read_access > )
    {
        const auto attr = attribute_by_type( 0xD0B1 );
        std::uint8_t buffer[ 4 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

        BOOST_CHECK( attr.access( read, 1 ) == bluetoe::details::attribute_access_result::read_not_permitted );
    }

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0xD0B1 >,
        bluetoe::fixed_value< std::int32_t, 0x08151213 >,
        bluetoe::notify
    > fixed_notifiable;

    BOOST_FIXTURE_TEST_CASE( fixed_notifiable_must_have_a_client_characteristic_configuration, access_attributes< fixed_notifiable > )
    {
        attribute_by_type( 0x2902 );
    }

BOOST_AUTO_TEST_SUITE_END()
