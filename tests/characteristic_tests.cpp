#include <bluetoe/characteristic.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {
    typedef bluetoe::characteristic<
    > simple_char;
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

BOOST_FIXTURE_TEST_CASE( the_characteristic_declaration_has_a_length_of_19, simple_char )
{
    const bluetoe::details::attribute char_declaration = attribute_at( 0 );
    std::uint8_t buffer[ 100 ];
    auto read = bluetoe::details::attribute_access_arguments::read( buffer );

    BOOST_REQUIRE( char_declaration.access );
    char_declaration.access( read );

    BOOST_CHECK_EQUAL( read.buffer_size, 19u );
}

