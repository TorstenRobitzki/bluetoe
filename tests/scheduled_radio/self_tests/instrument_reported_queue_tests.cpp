/**
 * @file instrument_reported_queue_tests.cpp
 *
 * The queue both instruments report through (decision 7): what it hands over, in what
 * order, and how a host sees what a full queue dropped.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "instrument/reported_queue.hpp"

#include <cstddef>
#include <vector>

using bluetoe::test_rig::reported_queue;

namespace {

    using queue_t = reported_queue< int, 5 >;

    // what a host does: asks until a batch comes back empty
    template < std::size_t PerBatch >
    std::vector< int > collect_all( queue_t& queue )
    {
        std::vector< int > result;

        for ( ;; )
        {
            const auto batch = queue.template collect< PerBatch >();

            if ( batch.count == 0 )
                return result;

            result.insert( result.end(), batch.items.begin(), batch.items.begin() + batch.count );
        }
    }
}

BOOST_AUTO_TEST_CASE( an_empty_queue_hands_over_an_empty_batch )
{
    queue_t queue;

    const auto batch = queue.collect< 2 >();

    BOOST_CHECK_EQUAL( batch.count, 0u );
    BOOST_CHECK_EQUAL( batch.first, 0u );
    BOOST_CHECK_EQUAL( batch.produced, 0u );
}

BOOST_AUTO_TEST_CASE( items_come_back_in_the_order_they_were_pushed )
{
    queue_t queue;

    for ( int item = 1; item != 5; ++item )
        queue.push( item );

    BOOST_TEST( collect_all< 2 >( queue ) == std::vector< int >( { 1, 2, 3, 4 } ), boost::test_tools::per_element() );
}

BOOST_AUTO_TEST_CASE( batches_carry_continuing_indices )
{
    queue_t queue;

    for ( int item = 0; item != 5; ++item )
        queue.push( item );

    const auto first  = queue.collect< 2 >();
    const auto second = queue.collect< 2 >();
    const auto third  = queue.collect< 2 >();
    const auto empty  = queue.collect< 2 >();

    BOOST_CHECK_EQUAL( first.first, 0u );
    BOOST_CHECK_EQUAL( first.count, 2u );
    BOOST_CHECK_EQUAL( second.first, 2u );
    BOOST_CHECK_EQUAL( second.count, 2u );
    BOOST_CHECK_EQUAL( third.first, 4u );
    BOOST_CHECK_EQUAL( third.count, 1u );
    BOOST_CHECK_EQUAL( empty.first, 5u );
    BOOST_CHECK_EQUAL( empty.count, 0u );
    BOOST_CHECK_EQUAL( empty.produced, 5u );
}

// a full queue drops the newest item, and counts it, so that the host sees the gap
BOOST_AUTO_TEST_CASE( a_full_queue_drops_the_newest_and_counts_them )
{
    queue_t queue;

    for ( int item = 0; item != 8; ++item )
        queue.push( item );

    const auto batch = queue.collect< 8 >();

    BOOST_CHECK_EQUAL( batch.produced, 8u );
    BOOST_CHECK_EQUAL( batch.count, 5u );
    BOOST_TEST( collect_all< 8 >( queue ).empty() );
}

BOOST_AUTO_TEST_CASE( what_was_collected_makes_room_for_more )
{
    queue_t queue;

    for ( int item = 0; item != 5; ++item )
        queue.push( item );

    BOOST_REQUIRE_EQUAL( queue.collect< 2 >().count, 2u );

    queue.push( 5 );
    queue.push( 6 );

    BOOST_TEST( collect_all< 8 >( queue ) == std::vector< int >( { 2, 3, 4, 5, 6 } ), boost::test_tools::per_element() );
}

BOOST_AUTO_TEST_CASE( clearing_forgets_the_items_and_the_counts )
{
    queue_t queue;

    for ( int item = 0; item != 7; ++item )
        queue.push( item );

    queue.clear();
    queue.push( 42 );

    const auto batch = queue.collect< 2 >();

    BOOST_CHECK_EQUAL( batch.first, 0u );
    BOOST_CHECK_EQUAL( batch.produced, 1u );
    BOOST_REQUIRE_EQUAL( batch.count, 1u );
    BOOST_CHECK_EQUAL( batch.items[ 0 ], 42 );
}
