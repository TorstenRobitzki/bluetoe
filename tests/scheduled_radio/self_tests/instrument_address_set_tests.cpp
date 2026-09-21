/**
 * @file instrument_address_set_tests.cpp
 *
 * The acceptance filter set both instruments keep.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "instrument/address_set.hpp"

using bluetoe::link_layer::device_address;
using bluetoe::test_rig::address_set;

namespace {

    const device_address first{ { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }, false };
    const device_address second{ { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }, true };
    const device_address third{ { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }, false };
}

BOOST_AUTO_TEST_CASE( a_new_set_is_empty )
{
    const address_set< 2 > set;

    BOOST_CHECK( set.empty() );
    BOOST_CHECK_EQUAL( set.size(), 0u );
    BOOST_CHECK( !set.contains( first ) );
}

BOOST_AUTO_TEST_CASE( an_added_address_is_contained )
{
    address_set< 2 > set;

    BOOST_CHECK( set.add( first ) );

    BOOST_CHECK( !set.empty() );
    BOOST_CHECK_EQUAL( set.size(), 1u );
    BOOST_CHECK( set.contains( first ) );
    BOOST_CHECK( !set.contains( third ) );
}

// the same six bytes as public and as random are two addresses
BOOST_AUTO_TEST_CASE( the_kind_of_an_address_tells_it_from_another )
{
    address_set< 2 > set;

    BOOST_CHECK( set.add( first ) );

    BOOST_CHECK( !set.contains( second ) );
}

BOOST_AUTO_TEST_CASE( adding_an_address_again_changes_nothing )
{
    address_set< 2 > set;

    BOOST_CHECK( set.add( first ) );
    BOOST_CHECK( set.add( first ) );

    BOOST_CHECK_EQUAL( set.size(), 1u );
}

BOOST_AUTO_TEST_CASE( a_full_set_refuses_a_new_address_and_keeps_the_old )
{
    address_set< 2 > set;

    BOOST_REQUIRE( set.add( first ) );
    BOOST_REQUIRE( set.add( second ) );

    BOOST_CHECK( !set.add( third ) );
    BOOST_CHECK( set.add( first ) );

    BOOST_CHECK_EQUAL( set.size(), 2u );
    BOOST_CHECK( set.contains( first ) );
    BOOST_CHECK( set.contains( second ) );
    BOOST_CHECK( !set.contains( third ) );
}
