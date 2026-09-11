#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "link/ring_buffer.hpp"
#include "link/serial_port.hpp"

#include <cstdint>
#include <deque>
#include <numeric>
#include <random>
#include <vector>

using bluetoe::test_rig::ring_buffer;

static_assert( bluetoe::test_rig::byte_ring_buffer< ring_buffer< std::uint8_t, 16 > > );

namespace {

    template < std::size_t Size >
    struct byte_buffer
    {
        ring_buffer< std::uint8_t, Size > buffer;

        void push( std::initializer_list< std::uint8_t > bytes )
        {
            buffer.push( std::data( bytes ), bytes.size() );
        }

        std::vector< std::uint8_t > pop( std::size_t count )
        {
            std::vector< std::uint8_t > result( count );
            buffer.pop( result.data(), count );

            return result;
        }
    };
}

BOOST_FIXTURE_TEST_CASE( a_new_buffer_is_empty, byte_buffer< 8 > )
{
    BOOST_CHECK_EQUAL( buffer.available(), 0u );
    BOOST_CHECK_EQUAL( buffer.free(), 8u );
}

BOOST_FIXTURE_TEST_CASE( pushed_elements_are_available, byte_buffer< 8 > )
{
    push( { 1, 2, 3 } );

    BOOST_CHECK_EQUAL( buffer.available(), 3u );
    BOOST_CHECK_EQUAL( buffer.free(), 5u );
}

BOOST_FIXTURE_TEST_CASE( elements_come_out_in_the_order_they_went_in, byte_buffer< 8 > )
{
    push( { 1, 2, 3 } );
    push( { 4, 5 } );

    BOOST_TEST( pop( 2 ) == std::vector< std::uint8_t >( { 1, 2 } ), boost::test_tools::per_element() );
    BOOST_TEST( pop( 3 ) == std::vector< std::uint8_t >( { 3, 4, 5 } ), boost::test_tools::per_element() );

    BOOST_CHECK_EQUAL( buffer.available(), 0u );
    BOOST_CHECK_EQUAL( buffer.free(), 8u );
}

BOOST_FIXTURE_TEST_CASE( the_whole_capacity_is_usable, byte_buffer< 4 > )
{
    push( { 1, 2, 3, 4 } );

    BOOST_CHECK_EQUAL( buffer.free(), 0u );
    BOOST_CHECK_EQUAL( buffer.available(), 4u );

    BOOST_TEST( pop( 4 ) == std::vector< std::uint8_t >( { 1, 2, 3, 4 } ), boost::test_tools::per_element() );

    BOOST_CHECK_EQUAL( buffer.free(), 4u );
    BOOST_CHECK_EQUAL( buffer.available(), 0u );
}

/*
 * A push that wraps around the end of the storage is the case the two-segment copy exists
 * for; the pop that reads it back wraps at the same place.
 */
BOOST_FIXTURE_TEST_CASE( a_push_wraps_around_the_end_of_the_storage, byte_buffer< 5 > )
{
    push( { 1, 2, 3 } );
    pop( 3 );

    push( { 4, 5, 6, 7 } );

    BOOST_CHECK_EQUAL( buffer.available(), 4u );
    BOOST_TEST( pop( 4 ) == std::vector< std::uint8_t >( { 4, 5, 6, 7 } ), boost::test_tools::per_element() );
}

/*
 * Every combination of fill level and wrap position, checked against a deque as the model,
 * for a size that is not a power of two.
 */
BOOST_AUTO_TEST_CASE( behaves_like_a_queue_across_many_wraps )
{
    ring_buffer< std::uint8_t, 7 > buffer;
    std::deque< std::uint8_t >     model;
    std::mt19937                   random( 42 );
    std::uint8_t                   next = 0;

    for ( int round = 0; round != 10000; ++round )
    {
        BOOST_REQUIRE_EQUAL( buffer.available(), model.size() );
        BOOST_REQUIRE_EQUAL( buffer.free(), 7 - model.size() );

        if ( random() % 2 )
        {
            const std::size_t count = random() % ( buffer.free() + 1 );

            std::vector< std::uint8_t > data( count );
            std::generate( data.begin(), data.end(), [&]{ return next++; } );

            buffer.push( data.data(), count );
            model.insert( model.end(), data.begin(), data.end() );
        }
        else
        {
            const std::size_t count = random() % ( buffer.available() + 1 );

            std::vector< std::uint8_t > data( count );
            buffer.pop( data.data(), count );

            BOOST_REQUIRE_EQUAL_COLLECTIONS( data.begin(), data.end(), model.begin(), model.begin() + count );
            model.erase( model.begin(), model.begin() + count );
        }
    }
}

/*
 * The rig's records are structs, pushed one at a time from the link layer context.
 */
BOOST_AUTO_TEST_CASE( holds_elements_that_are_not_bytes )
{
    struct record
    {
        std::uint32_t   sequence_number;
        int             payload;
    };

    ring_buffer< record, 3 > buffer;

    const record first  = { 1, 10 };
    const record second = { 2, 20 };

    buffer.push( &first, 1 );
    buffer.push( &second, 1 );

    record out[ 2 ];
    buffer.pop( out, 2 );

    BOOST_CHECK_EQUAL( out[ 0 ].sequence_number, 1u );
    BOOST_CHECK_EQUAL( out[ 0 ].payload, 10 );
    BOOST_CHECK_EQUAL( out[ 1 ].sequence_number, 2u );
    BOOST_CHECK_EQUAL( out[ 1 ].payload, 20 );
}
