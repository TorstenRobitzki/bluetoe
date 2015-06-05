#include <iostream>
#include <bluetoe/scattered_access.hpp>
#include "hexdump.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace blued = bluetoe::details;

namespace {
    static const std::uint8_t a[] = { 1, 2 };
    static const std::uint8_t b[] = { 3 };
    static const std::uint8_t c[] = { 4, 5, 6 };
}

BOOST_AUTO_TEST_CASE( full_access )
{
    static const std::uint8_t expected_result[] = { 1, 2, 3, 4, 5, 6 };

    std::uint8_t result[ 6 ] = { 0 };

    blued::scattered_read_access( 0, a, b, c, &result[ 0 ], sizeof( result ) );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_result ), std::end( expected_result ), std::begin( result ), std::end( result ) );
}

BOOST_AUTO_TEST_CASE( truncated )
{
    static const std::uint8_t expected_result[] = { 1, 2, 3, 4, 5 };

    std::uint8_t result[ 6 ] = { 0 };

    blued::scattered_read_access( 0, a, b, c, &result[ 0 ], sizeof( result ) - 1 );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_result ), std::end( expected_result ), std::begin( result ), std::end( result ) - 1 );
    BOOST_CHECK_EQUAL( result[ 5 ], 0 );
}

BOOST_AUTO_TEST_CASE( truncated_in_b )
{
    static const std::uint8_t expected_result[] = { 1, 2, 3 };

    std::uint8_t result[ 6 ] = { 0 };

    blued::scattered_read_access( 0, a, b, c, &result[ 0 ], sizeof( result ) - 3 );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_result ), std::end( expected_result ), std::begin( result ), std::end( result ) - 3 );
    BOOST_CHECK_EQUAL( result[ 3 ], 0 );
}

BOOST_AUTO_TEST_CASE( truncated_to_zero )
{
    std::uint8_t result[ 6 ] = { 0 };

    blued::scattered_read_access( 0, a, b, c, &result[ 0 ], 0 );

    BOOST_CHECK_EQUAL( result[ 0 ], 0 );
}

BOOST_AUTO_TEST_CASE( full_access_with_offset )
{
    static const std::uint8_t expected_result[] = { 2, 3, 4, 5, 6 };

    std::uint8_t result[ 5 ] = { 0 };

    blued::scattered_read_access( 1, a, b, c, &result[ 0 ], sizeof( result ) );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_result ), std::end( expected_result ), std::begin( result ), std::end( result ) );
}

BOOST_AUTO_TEST_CASE( truncated_offset )
{
    static const std::uint8_t expected_result[] = { 2, 3, 4, 5 };

    std::uint8_t result[ 5 ] = { 0 };

    blued::scattered_read_access( 1, a, b, c, &result[ 0 ], sizeof( result ) - 1 );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_result ), std::end( expected_result ), std::begin( result ), std::end( result ) - 1 );
    BOOST_CHECK_EQUAL( result[ 4 ], 0 );
}

BOOST_AUTO_TEST_CASE( truncated_with_offset_into_c )
{
    std::uint8_t result[ 2 ] = { 0 };

    blued::scattered_read_access( 4, a, b, c, &result[ 0 ], 1 );

    BOOST_CHECK_EQUAL( result[ 0 ], 5 );
    BOOST_CHECK_EQUAL( result[ 1 ], 0 );
}
