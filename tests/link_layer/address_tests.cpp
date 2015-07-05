#include <bluetoe/link_layer/address.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

BOOST_FIXTURE_TEST_CASE( is_default_constructable, bluetoe::link_layer::address )
{
    BOOST_CHECK_EQUAL( *this,  bluetoe::link_layer::address( { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } ) );
}

BOOST_AUTO_TEST_CASE( is_initalizable_by_list )
{
    bluetoe::link_layer::address a( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
}

BOOST_AUTO_TEST_CASE( is_initalizable_by_array )
{
    static const std::uint8_t arr[ 6 ] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

    BOOST_CHECK_EQUAL( bluetoe::link_layer::address( &arr[ 0 ] ), bluetoe::link_layer::address(  { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } ) );
}

BOOST_AUTO_TEST_CASE( outputable )
{
    boost::test_tools::output_test_stream output;

    output << bluetoe::link_layer::address( { 0x01, 0xFF, 0x10, 0x04, 0x55, 0xAb } );

    BOOST_CHECK( output.is_equal( "01:ff:10:04:55:ab" ) );
}

BOOST_AUTO_TEST_CASE( is_equal_compareble )
{
    const bluetoe::link_layer::address a( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
    const bluetoe::link_layer::address b( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
    const bluetoe::link_layer::address c( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x07 } );
    const bluetoe::link_layer::address d( { 0x10, 0x02, 0x03, 0x04, 0x05, 0x06 } );

    BOOST_CHECK_EQUAL( a, a );
    BOOST_CHECK_EQUAL( a, b );
    BOOST_CHECK_EQUAL( b, a );
    BOOST_CHECK( !( a == c ) );
    BOOST_CHECK( !( a == d ) );
}

BOOST_AUTO_TEST_CASE( is_unequal_compareble )
{
    const bluetoe::link_layer::address a( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
    const bluetoe::link_layer::address b( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
    const bluetoe::link_layer::address c( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x07 } );
    const bluetoe::link_layer::address d( { 0x10, 0x02, 0x03, 0x04, 0x05, 0x06 } );

    BOOST_CHECK( !( a != a ) );
    BOOST_CHECK( !( a != b ) );
    BOOST_CHECK_NE( a, c );
    BOOST_CHECK_NE( a, d );
}

BOOST_AUTO_TEST_CASE( gives_access_to_the_msb )
{
    BOOST_CHECK_EQUAL( 0x45, bluetoe::link_layer::address( { 0x00, 0x00, 0x00, 0x00, 0x00, 0x45 } ).msb() );
}

BOOST_AUTO_TEST_CASE( implements_iterator_access )
{
    bluetoe::link_layer::address a( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
    const std::uint8_t exepect_values[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

    BOOST_CHECK_EQUAL_COLLECTIONS( a.begin(), a.end(), std::begin( exepect_values ), std::end( exepect_values ) );
}
