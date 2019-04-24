#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_radio.hpp"

BOOST_AUTO_TEST_CASE( detect_equal_pdus )
{
    BOOST_CHECK( test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, 0x01, 0x02 } ) );
}

BOOST_AUTO_TEST_CASE( detect_unequal_pdus )
{
    BOOST_CHECK( !test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, 0x01, 0x03 } ) );
}

BOOST_AUTO_TEST_CASE( detect_unequal_size_pdus )
{
    BOOST_CHECK( !test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, 0x01 } ) );
    BOOST_CHECK( !test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, 0x01, 0x02, 0x03 } ) );
}

BOOST_AUTO_TEST_CASE( wild_card )
{
    BOOST_CHECK( test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, 0x01, test::X } ) );
    BOOST_CHECK( test::check_pdu( { 0x00, 0x01, 0x02 }, { test::X, 0x01, 0x02 } ) );
}

BOOST_AUTO_TEST_CASE( and_so_on )
{
    BOOST_CHECK( test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, test::and_so_on } ) );
    BOOST_CHECK( test::check_pdu( { 0x00, 0x01, 0x02 }, { 0x00, 0x01, 0x02, test::and_so_on } ) );
}

BOOST_AUTO_TEST_CASE( mixed_X_and_so_on )
{
    BOOST_CHECK(  test::check_pdu( { 0x00, 0x01, 0x02 }, { test::X, 0x01, test::and_so_on } ) );
    BOOST_CHECK( !test::check_pdu( { 0x00, 0x01, 0x02 }, { test::X, 0x00, test::and_so_on } ) );
}

BOOST_AUTO_TEST_CASE( pretty_print_empty )
{
    BOOST_CHECK_EQUAL( test::pretty_print_pattern( {} ), "" );
}

BOOST_AUTO_TEST_CASE( pretty_print_without_wildcards )
{
    BOOST_CHECK_EQUAL( test::pretty_print_pattern( { 0x01, 0x02, 0xaa, 0xff } ), "01 02 aa ff" );
}

BOOST_AUTO_TEST_CASE( pretty_print_with_line_wraps )
{
    BOOST_CHECK_EQUAL(
        test::pretty_print_pattern(
            {
                0x01, 0x02, 0xaa, 0xff, 0x01, 0x02, 0xaa, 0xff, 0x01, 0x02, 0xaa, 0xff, 0x01, 0x02, 0xaa, 0xff,
                0x01, 0x02, 0xaa, 0xff, 0x01, 0x02, 0xaa, 0xff, 0x01, 0x02, 0xaa, 0xff, 0x01, 0x02, 0xaa, 0xff,
                0x01, 0x02, 0xaa
            }
        ),
        "01 02 aa ff 01 02 aa ff 01 02 aa ff 01 02 aa ff\n"
        "01 02 aa ff 01 02 aa ff 01 02 aa ff 01 02 aa ff\n"
        "01 02 aa"
    );
}

BOOST_AUTO_TEST_CASE( pretty_print_with_wildcards )
{
    BOOST_CHECK_EQUAL(
        test::pretty_print_pattern(
            {
                0x01, test::X, 0xaa, 0xff, 0x01, 0x02, test::and_so_on
            }
        ),
        "01 XX aa ff 01 02 ..."
    );
}