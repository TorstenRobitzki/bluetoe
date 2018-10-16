#include <bluetoe/ring_buffer.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

struct small_ring : bluetoe::link_layer::pdu_ring_buffer< 50 >
{
    small_ring() : bluetoe::link_layer::pdu_ring_buffer< 50 >( &buffer[ 0 ] )
    {
    }

    std::uint8_t buffer[ size ];
};

BOOST_FIXTURE_TEST_CASE( newly_constructed_is_empty, small_ring )
{
    BOOST_CHECK_EQUAL( next_end().size, 0u );
}

BOOST_FIXTURE_TEST_CASE( newly_contructed_contains_not_more_than_one, small_ring )
{
    BOOST_CHECK( !more_than_one() );
}

BOOST_FIXTURE_TEST_CASE( allocating_from_empty, small_ring )
{
    BOOST_CHECK_EQUAL( alloc_front( buffer, 30 ).size, 30u );
}

BOOST_FIXTURE_TEST_CASE( no_room_no_pdu, small_ring )
{
    BOOST_CHECK_EQUAL( alloc_front( buffer, 51 ).size, 0u );
}

BOOST_FIXTURE_TEST_CASE( allocates_the_fist_bytes_of_the_buffer, small_ring )
{
    BOOST_CHECK_EQUAL( alloc_front( buffer, 3 ).buffer, buffer );
    BOOST_CHECK_EQUAL( alloc_front( buffer, 20 ).buffer, buffer );
    BOOST_CHECK_EQUAL( alloc_front( buffer, 50 ).buffer, buffer );
}

BOOST_FIXTURE_TEST_CASE( allocating_will_hand_out_different_buffer_positions, small_ring )
{
    auto p1 = alloc_front( buffer, 40 );
    BOOST_CHECK_EQUAL( p1.buffer, buffer );
    BOOST_CHECK_EQUAL( p1.size, 40u );

    p1.buffer[ 1 ] = 1;
    push_front( buffer, p1 );

    auto p2 = alloc_front( buffer, 40 );
    BOOST_CHECK_EQUAL( p2.buffer, &buffer[ 3 ] );
    BOOST_CHECK_EQUAL( p2.size, 40u );

    p2.buffer[ 1 ] = 18;
    push_front( buffer, p2 );

    auto p3 = alloc_front( buffer, 15 );
    BOOST_CHECK_EQUAL( p3.buffer, &buffer[ 23 ] );
    BOOST_CHECK_EQUAL( p3.size, 15u );
}

BOOST_FIXTURE_TEST_CASE( storing_at_the_front_will_result_in_allocation_failure, small_ring )
{
    auto p1 = alloc_front( buffer, 40 );
    p1.buffer[ 1 ] = 1;
    push_front( buffer, p1 );

    auto p2 = alloc_front( buffer, 40 );
    p2.buffer[ 1 ] = 18;
    push_front( buffer, p2 );

    BOOST_CHECK_EQUAL( alloc_front( buffer, 30 ).size, 0u );
}

/*
 * Two elements of size 18 stored at the beginning of the buffer
 */
struct full_ring : small_ring
{
    full_ring()
    {
        auto p1 = alloc_front( buffer, 20 );
        p1.buffer[ 1 ] = 16;
        push_front( buffer, p1 );

        auto p2 = alloc_front( buffer, 20 );
        p2.buffer[ 1 ] = 16;
        push_front( buffer, p2 );

        BOOST_REQUIRE_EQUAL( alloc_front( buffer, 19 ).size, 0u );
    }
};

BOOST_FIXTURE_TEST_CASE( after_freeing_at_the_end_of_a_full_ring_there_is_room_again, full_ring )
{
    pop_end( buffer );
    // if the buffer is splitted, the size of possible allocation is reduced by one
    BOOST_CHECK_EQUAL( alloc_front( buffer, 17 ).size, 17u );
    BOOST_CHECK_EQUAL( alloc_front( buffer, 17 ).buffer, buffer );
}

BOOST_FIXTURE_TEST_CASE( full_ring_contains_more_than_one, full_ring )
{
    BOOST_CHECK( more_than_one() );
}

/*
 * Ring is splitted at pos 36 and empty
 */
struct empty_split_ring : full_ring
{
    empty_split_ring()
    {
        pop_end( buffer );
        pop_end( buffer );
    }
};

BOOST_FIXTURE_TEST_CASE( splited_empty, empty_split_ring )
{
    BOOST_CHECK_EQUAL( next_end().size, 0u );
}

BOOST_FIXTURE_TEST_CASE( when_splitted_full_allocation_not_possible, empty_split_ring )
{
    BOOST_CHECK_EQUAL( alloc_front( buffer, 36 ).size, 0u );
    BOOST_CHECK_EQUAL( alloc_front( buffer, 35 ).size, 35u );
}

BOOST_FIXTURE_TEST_CASE( empty_split_ring_contains_not_more_than_one, empty_split_ring )
{
    BOOST_CHECK( !more_than_one() );
}

/*
 * After the ring was splitted in the middle, the maximum possible block of 35 bytes are
 * allocated at the end of the ring.
 */
struct one_block_at_the_end : empty_split_ring
{
    one_block_at_the_end()
    {
        auto p = alloc_front( buffer, 35 );
        p.buffer[ 1 ] = 33;
        push_front( buffer, p );
    }
};

BOOST_FIXTURE_TEST_CASE( max_alloc_front_after_split, one_block_at_the_end )
{
    BOOST_CHECK_EQUAL( alloc_front( buffer, 16 ).size, 0u );
    BOOST_CHECK_EQUAL( alloc_front( buffer, 15 ).size, 15u );
}

BOOST_FIXTURE_TEST_CASE( access_to_allocated_block_at_the_beginning, one_block_at_the_end )
{
    BOOST_CHECK_EQUAL( next_end().size, 35u );
    BOOST_CHECK_EQUAL( next_end().buffer - &buffer[ 0 ], 0 );
}

BOOST_FIXTURE_TEST_CASE( one_block_at_the_end_contains_not_more_than_one, one_block_at_the_end )
{
    BOOST_CHECK( !more_than_one() );
}

/*
 * If the allocated chunk is small enough, it will be allocated from the end of the memeory, without wrapping
 */
struct one_small_block_at_the_end : empty_split_ring
{
    one_small_block_at_the_end()
    {
        auto p = alloc_front( buffer, 3 );
        p.buffer[ 1 ] = 1;
        push_front( buffer, p );
    }
};

BOOST_FIXTURE_TEST_CASE( access_to_allocated_small_block_at_the_end, one_small_block_at_the_end )
{
    BOOST_CHECK_EQUAL( next_end().size, 3u );
    BOOST_CHECK_EQUAL( next_end().buffer - &buffer[ 0 ], 36u );
}

/*
 * When allocating to the end of the buffer and then pop all elements out of the buffer,
 * the buffer is splitted at the end of the buffer.
 */
struct splitted_at_end : one_block_at_the_end
{
    splitted_at_end()
    {
        auto p = alloc_front( buffer, 15 );
        p.buffer[ 1 ] = 13;
        push_front( buffer, p );

        pop_end( buffer );
        pop_end( buffer );
    }
};

BOOST_FIXTURE_TEST_CASE( nearly_max_alloc_from_empty_buffer, splitted_at_end )
{
    BOOST_CHECK_EQUAL( alloc_front( buffer, 50 ).size, 0u );
    BOOST_CHECK_EQUAL( alloc_front( buffer, 49 ).size, 49u );
}
