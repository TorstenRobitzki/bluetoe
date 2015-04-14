#include "test_services.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <iterator>

BOOST_AUTO_TEST_CASE( service_without_any_characteristic_results_in_one_attribute )
{
    const auto number = empty_service::number_of_attributes;
    BOOST_CHECK_EQUAL( 1u, number );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service )
{
    const auto attr = empty_service::attribute_at( 0 );
    BOOST_CHECK_EQUAL( 0x2800, attr.uuid );
}

static const std::uint8_t expected_uuid[ 16 ] = { 0xF0, 0x42, 0x6E, 0x52, 0x44, 0x50, 0x4F, 0x3B, 0xB0, 0x58, 0x5B, 0xAB, 0x11, 0x91, 0xD9, 0x2A };

static void check_service_uuid( const bluetoe::details::attribute_access_arguments& args, std::size_t size = sizeof( expected_uuid ) )
{
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::begin( expected_uuid ) + size, args.buffer, args.buffer + args.buffer_size );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read )
{
    const auto attr = empty_service::attribute_at( 0 );
    BOOST_REQUIRE( attr.access );

    std::uint8_t buffer[ 16 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer );
    const auto access_result = attr.access( read );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    check_service_uuid( read );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read_buffer_larger )
{
    const auto attr = empty_service::attribute_at( 0 );

    std::uint8_t buffer[ 20 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer );
    const auto access_result = attr.access( read );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::success );
    check_service_uuid( read );
}

BOOST_AUTO_TEST_CASE( first_attribute_is_the_primary_service_and_can_be_read_buffer_to_small )
{
    const auto attr = empty_service::attribute_at( 0 );

    std::uint8_t buffer[ 15 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer );
    const auto access_result = attr.access( read );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::read_truncated );
    check_service_uuid( read, sizeof( buffer ) );
}

BOOST_AUTO_TEST_CASE( write_to_primary_service )
{
    const auto attr = empty_service::attribute_at( 0 );

    std::uint8_t buffer[] = { 1, 2, 3 };
    auto write = bluetoe::details::attribute_access_arguments::write( buffer );
    const auto access_result = attr.access( write );

    BOOST_CHECK( access_result == bluetoe::details::attribute_access_result::write_not_permitted );
}

BOOST_FIXTURE_TEST_CASE( accessing_all_attributes, service_with_3_characteristics )
{
    static constexpr std::size_t expected_number_of_attributes = 7u;
    BOOST_REQUIRE_EQUAL( unsigned( number_of_attributes ), expected_number_of_attributes );

    BOOST_CHECK_EQUAL( 0x2800, attribute_at( 0 ).uuid );
    BOOST_CHECK_EQUAL( 0x2803, attribute_at( 1 ).uuid );
    BOOST_CHECK_EQUAL( 0x0001, attribute_at( 2 ).uuid );
    BOOST_CHECK_EQUAL( 0x2803, attribute_at( 3 ).uuid );
    BOOST_CHECK_EQUAL( 0x0001, attribute_at( 4 ).uuid );
    BOOST_CHECK_EQUAL( 0x2803, attribute_at( 5 ).uuid );
    BOOST_CHECK_EQUAL( 0x0815, attribute_at( 6 ).uuid );
}
