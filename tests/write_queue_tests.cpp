#include <iostream>
#include <bluetoe/write_queue.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace blued = bluetoe::details;

BOOST_FIXTURE_TEST_CASE( empty_write_queue_is_instanceiable, blued::write_queue< blued::no_such_type > )
{
}

typedef blued::write_queue< bluetoe::shared_write_queue< 100 > > queue_100;

char client;

BOOST_FIXTURE_TEST_CASE( can_allocate_98_bytes, queue_100 )
{
    std::uint8_t* const p = allocate_from_write_queue( 98, client );

    BOOST_CHECK( p != nullptr );
}

BOOST_FIXTURE_TEST_CASE( can_allocate_98_bytes_but_not_a_single_byte_more, queue_100 )
{
    allocate_from_write_queue( 98, client );
    std::uint8_t* const p = allocate_from_write_queue( 1, client );

    BOOST_CHECK( p == nullptr );
}

BOOST_FIXTURE_TEST_CASE( can_allocate_multiple_times, queue_100 )
{
    std::uint8_t* p = nullptr;

    for ( int times = 5; times; --times )
    {
        std::uint8_t* const new_p = allocate_from_write_queue( 15, client );

        BOOST_CHECK( new_p != nullptr );
        BOOST_CHECK( new_p != p );

        p = new_p;
    }
}

BOOST_FIXTURE_TEST_CASE( can_allocate_after_releasing, queue_100 )
{
    for ( int times = 5; times; --times )
    {
        std::uint8_t* const p = allocate_from_write_queue( 98, client );

        BOOST_CHECK( p != nullptr );
        free_write_queue( client );
    }
}

BOOST_FIXTURE_TEST_CASE( no_first_element_in_empty_queue, queue_100 )
{
    BOOST_CHECK( first_write_queue_element( client ) == nullptr );
}

BOOST_FIXTURE_TEST_CASE( queue_can_be_iterated, queue_100 )
{
    static const std::uint8_t test1[ 5 ] = { 1, 2, 3, 4, 5 };
    static const std::uint8_t test2[ 5 ] = { 6, 7, 8, 8, 8 };
    static const std::uint8_t test3[ 5 ] = { 100, 101, 102, 103, 104 };

    std::uint8_t* p1 = allocate_from_write_queue( 5, client );
    std::copy( std::begin( test1 ), std::end( test1 ), p1 );

    std::uint8_t* p2 = allocate_from_write_queue( 5, client );
    std::copy( std::begin( test2 ), std::end( test2 ), p2 );

    std::uint8_t* p3 = allocate_from_write_queue( 5, client );
    std::copy( std::begin( test3 ), std::end( test3 ), p3 );

    std::uint8_t* ele1 = first_write_queue_element( client );
    BOOST_CHECK_EQUAL_COLLECTIONS( ele1, ele1 + 5, std::begin( test1 ), std::end( test1 ) );

    std::uint8_t* ele2 = next_write_queue_element( ele1, client );
    BOOST_CHECK_EQUAL_COLLECTIONS( ele2, ele2 + 5, std::begin( test2 ), std::end( test2 ) );

    std::uint8_t* ele3 = next_write_queue_element( ele2, client );
    BOOST_CHECK_EQUAL_COLLECTIONS( ele3, ele3 + 5, std::begin( test3 ), std::end( test3 ) );

    BOOST_CHECK( next_write_queue_element( ele3, client ) == nullptr );
}

struct locked_by_client1 : queue_100
{
    locked_by_client1()
    {
        BOOST_CHECK( allocate_from_write_queue( 15, client1 ) != nullptr );
    }

    char client1, client2;
};

BOOST_FIXTURE_TEST_CASE( seems_to_be_full, locked_by_client1 )
{
    BOOST_CHECK( allocate_from_write_queue( 15, client2 ) == nullptr );
}

BOOST_FIXTURE_TEST_CASE( but_also_seems_to_be_empty, locked_by_client1 )
{
    BOOST_CHECK( first_write_queue_element( client2 ) == nullptr );
}

BOOST_FIXTURE_TEST_CASE( can_not_be_freed_by_othere_clients, locked_by_client1 )
{
    free_write_queue( client2 );
    BOOST_CHECK( allocate_from_write_queue( 15, client2 ) == nullptr );
}

BOOST_FIXTURE_TEST_CASE( can_still_be_used_by_locking_client, locked_by_client1 )
{
    BOOST_CHECK( allocate_from_write_queue( 15, client1 ) != nullptr );
}

BOOST_FIXTURE_TEST_CASE( can_be_freed_and_allocated_again, locked_by_client1 )
{
    free_write_queue( client1 );
    BOOST_CHECK( allocate_from_write_queue( 15, client2 ) != nullptr );
}
