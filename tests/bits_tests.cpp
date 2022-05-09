#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/bits.hpp>

BOOST_AUTO_TEST_CASE( no_distance )
{
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0u, 0u ) == 0 );
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0xfff2u, 0xfff2u ) == 0 );
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0x4444u, 0x4444u ) == 0 );
}

BOOST_AUTO_TEST_CASE( positive_distance )
{
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0u, 1u ) == 1 );
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0x0ff2u, 0x4ff2u ) == 0x4000 );
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0xf444u, 0x0444u ) == 0x1000 );
}

BOOST_AUTO_TEST_CASE( negative_distance )
{
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 1u, 0u ) == -1 );
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0x4ff2u, 0x0ff2u ) == -0x4000 );
    BOOST_TEST( bluetoe::details::distance< std::uint16_t >( 0x0444u, 0xf444u ) == -0x1000 );
}

BOOST_AUTO_TEST_CASE( no_distance_n )
{
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0xffffff, 0xffffff ) ) == 0 );
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0x0, 0x0 ) ) == 0 );
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0x800000, 0x800000 ) ) == 0 );
}

BOOST_AUTO_TEST_CASE( positive_distance_n )
{
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0xffffff, 0x7a ) ) == 0x7b );
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0x0, 0x7fffff ) ) == 0x7fffff );
}

BOOST_AUTO_TEST_CASE( negative_distance_n )
{
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0x7a, 0xffffff ) ) == -0x7b );
    BOOST_TEST( ( bluetoe::details::distance_n< 24u, std::uint32_t >( 0x0, 0x800001 ) ) == -0x7fffff );
}
