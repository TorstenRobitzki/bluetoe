#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "link/serialize.hpp"

#include <cstdint>
#include <vector>

using namespace bluetoe::test_rig;
using bluetoe::link_layer::abs_time;
using bluetoe::link_layer::delta_time;

namespace {

    static_assert( sink< buffer_sink > );
    static_assert( source< buffer_source > );

    /*
     * Serialises into a buffer, hands back the bytes, and deserialises them again.
     */
    struct round_trip
    {
        std::array< std::uint8_t, 64 > storage = {};

        template < typename T >
        std::vector< std::uint8_t > encode( const T& value )
        {
            buffer_sink out( storage );
            BOOST_REQUIRE( serialize( out, value ) );

            return { storage.begin(), storage.begin() + out.size() };
        }

        template < typename T >
        T decode( const std::vector< std::uint8_t >& encoded )
        {
            buffer_source in( encoded.data(), encoded.size() );
            T value;

            BOOST_REQUIRE( deserialize( in, value ) );
            BOOST_CHECK_EQUAL( in.remaining(), 0u );

            return value;
        }

        template < typename T >
        void check( const T& value, const std::vector< std::uint8_t >& expected )
        {
            const auto encoded = encode( value );

            BOOST_TEST( encoded == expected, boost::test_tools::per_element() );
            BOOST_CHECK( decode< T >( encoded ) == value );
        }
    };

    enum class colour : std::uint8_t { red = 1, green = 2 };

    enum class wide : std::uint16_t { value = 0x1234 };
}

BOOST_FIXTURE_TEST_CASE( integers_are_little_endian_at_their_width, round_trip )
{
    check< std::uint8_t >( 0x12, { 0x12 } );
    check< std::uint16_t >( 0x1234, { 0x34, 0x12 } );
    check< std::uint32_t >( 0x12345678, { 0x78, 0x56, 0x34, 0x12 } );
}

BOOST_FIXTURE_TEST_CASE( bool_is_one_byte, round_trip )
{
    check( true, { 0x01 } );
    check( false, { 0x00 } );

    BOOST_CHECK( decode< bool >( { 0x02 } ) );
}

BOOST_FIXTURE_TEST_CASE( an_enum_is_its_underlying_integer, round_trip )
{
    check( colour::green, { 0x02 } );
    check( wide::value, { 0x34, 0x12 } );
}

BOOST_FIXTURE_TEST_CASE( times_are_their_microseconds, round_trip )
{
    const auto encoded = encode( abs_time( 0x01020304 ) );
    BOOST_TEST( encoded == std::vector< std::uint8_t >( { 0x04, 0x03, 0x02, 0x01 } ), boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( decode< abs_time >( encoded ).data(), 0x01020304u );

    check( delta_time::msec( 1 ), { 0xe8, 0x03, 0x00, 0x00 } );
}

BOOST_FIXTURE_TEST_CASE( arrays_pairs_and_tuples_are_their_elements_in_order, round_trip )
{
    check( std::array< std::uint8_t, 3 >{ 1, 2, 3 }, { 1, 2, 3 } );
    check( std::array< std::uint16_t, 2 >{ 0x0102, 0x0304 }, { 0x02, 0x01, 0x04, 0x03 } );
    check( std::pair< std::uint8_t, std::uint16_t >{ 1, 0x0203 }, { 0x01, 0x03, 0x02 } );
    check( std::tuple< std::uint8_t, bool, std::uint16_t >{ 7, true, 0x0102 }, { 0x07, 0x01, 0x02, 0x01 } );
    check( std::tuple<>{}, {} );
}

BOOST_FIXTURE_TEST_CASE( a_byte_sequence_carries_its_length, round_trip )
{
    const std::uint8_t data[] = { 0xaa, 0xbb, 0xcc };

    const auto encoded = encode( std::span< const std::uint8_t >( data ) );
    BOOST_TEST( encoded == std::vector< std::uint8_t >( { 0x03, 0x00, 0xaa, 0xbb, 0xcc } ), boost::test_tools::per_element() );

    const auto decoded = decode< bytes< 8 > >( encoded );
    BOOST_CHECK_EQUAL( decoded.size, 3u );
    BOOST_CHECK_EQUAL_COLLECTIONS( decoded.data.begin(), decoded.data.begin() + 3, data, data + 3 );

    bytes< 8 > empty;
    check( empty, { 0x00, 0x00 } );
}

BOOST_AUTO_TEST_CASE( a_byte_sequence_longer_than_its_bound_is_rejected )
{
    const std::uint8_t encoded[] = { 0x03, 0x00, 0xaa, 0xbb, 0xcc };

    buffer_source in( encoded, sizeof( encoded ) );
    bytes< 2 >    value;

    BOOST_CHECK( !deserialize( in, value ) );
}

BOOST_AUTO_TEST_CASE( a_truncated_source_fails_instead_of_reading_past_the_end )
{
    const std::uint8_t encoded[] = { 0x34, 0x12, 0x78 };

    buffer_source in( encoded, sizeof( encoded ) );

    std::uint16_t first;
    std::uint32_t second;

    BOOST_CHECK( deserialize( in, first ) );
    BOOST_CHECK_EQUAL( first, 0x1234 );
    BOOST_CHECK( !deserialize( in, second ) );
}

BOOST_AUTO_TEST_CASE( a_truncated_tuple_fails_as_a_whole )
{
    const std::uint8_t encoded[] = { 0x01, 0x02 };

    buffer_source in( encoded, sizeof( encoded ) );
    std::tuple< std::uint8_t, std::uint16_t > value;

    BOOST_CHECK( !deserialize( in, value ) );
}

BOOST_AUTO_TEST_CASE( a_full_sink_fails_instead_of_truncating )
{
    std::array< std::uint8_t, 3 > storage = {};
    buffer_sink out( storage );

    BOOST_CHECK( serialize( out, std::uint16_t( 0x0102 ) ) );
    BOOST_CHECK( !serialize( out, std::uint16_t( 0x0304 ) ) );
    BOOST_CHECK_EQUAL( out.size(), 2u );
}

BOOST_AUTO_TEST_CASE( a_sequence_that_does_not_fit_the_sink_fails )
{
    std::array< std::uint8_t, 4 > storage = {};
    buffer_sink out( storage );

    const std::uint8_t data[] = { 1, 2, 3 };

    BOOST_CHECK( !serialize( out, std::span< const std::uint8_t >( data ) ) );
}
