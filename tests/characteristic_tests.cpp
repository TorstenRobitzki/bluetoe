#include <bluetoe/characteristic.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {
    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >
    > simple_char;

   typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0xD0B1 >
    > short_uuid_char;

}

BOOST_AUTO_TEST_CASE( even_the_simplest_characteristic_has_at_list_2_attributes )
{
    BOOST_CHECK_GE( std::size_t( simple_char::number_of_attributes ), 2 );
}

BOOST_FIXTURE_TEST_CASE( the_first_attribute_is_the_characteristic_declaration, simple_char )
{
    const bluetoe::details::attribute char_declaration = attribute_at( 0 );

    BOOST_CHECK_EQUAL( char_declaration.uuid, 0x2803 );
}

BOOST_AUTO_TEST_SUITE( characteristic_declaration_access )

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_has_a_length_of_19, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer );

        BOOST_REQUIRE( char_declaration.access );
        char_declaration.access( read );

        BOOST_CHECK_EQUAL( read.buffer_size, 19u );
    }

    BOOST_FIXTURE_TEST_CASE( the_short_characteristic_declaration_has_a_length_of_5, short_uuid_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer );

        char_declaration.access( read );

        BOOST_CHECK_EQUAL( read.buffer_size, 5u );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_uuid, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer );

        char_declaration.access( read );

        // D0B10674-6DDD-4B59-89CA-A009B78C956B
        static const std::uint8_t expected_uuid[] = { 0xD0, 0xB1, 0x06, 0x74, 0x6D, 0xDD, 0x4B, 0x59, 0x89, 0xCA, 0xA0, 0x09, 0xB7, 0x8C, 0x95, 0x6B };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 3 ], &buffer[ 19 ] );
    }

    BOOST_FIXTURE_TEST_CASE( read_char_declaration_buffer_to_small, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at( 0 );
        std::uint8_t buffer[ 17 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer );

        BOOST_CHECK( bluetoe::details::attribute_access_result::read_truncated == char_declaration.access( read ) );
        BOOST_CHECK_EQUAL( read.buffer_size, 17u );
        static const std::uint8_t expected_uuid[] = { 0xD0, 0xB1, 0x06, 0x74, 0x6D, 0xDD, 0x4B, 0x59, 0x89, 0xCA, 0xA0, 0x09, 0xB7, 0x8C };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 3 ], &buffer[ 17 ] );
    }

    BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_contains_the_16bit_uuid, short_uuid_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at( 0 );
        std::uint8_t buffer[ 100 ];
        auto read = bluetoe::details::attribute_access_arguments::read( buffer );

        char_declaration.access( read );

        static const std::uint8_t expected_uuid[] = { 0xB1, 0xD0 };
        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_uuid ), std::end( expected_uuid ), &buffer[ 3 ], &buffer[ 5 ] );
    }

    BOOST_FIXTURE_TEST_CASE( char_declaration_is_not_writable, simple_char )
    {
        const bluetoe::details::attribute char_declaration = attribute_at( 0 );
        std::uint8_t buffer[ 4 ];
        auto write = bluetoe::details::attribute_access_arguments::write( buffer );

        BOOST_CHECK( char_declaration.access( write ) == bluetoe::details::attribute_access_result::write_not_permitted );
    }

BOOST_AUTO_TEST_SUITE_END()
