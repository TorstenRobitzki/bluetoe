#include <iostream>

#include <bluetoe/filter.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/uuid.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace blued = bluetoe::details;

namespace {
    template < class UUID >
    blued::uuid_filter fixture()
    {
        return blued::uuid_filter( &UUID::bytes[ 0 ], UUID::is_128bit );
    }

    typedef bluetoe::details::uuid< 0x1, 0x2, 0x3, 0x4, 0x5 > big_uuid;

    blued::attribute_access_result equal_to_big_uuid( blued::attribute_access_arguments& args )
    {
        if ( args.type == blued::attribute_access_type::compare_128bit_uuid )
        {
            assert( args.buffer_size == 16 );

            if ( std::equal( std::begin( big_uuid::bytes ), std::end( big_uuid::bytes ), args.buffer ) )
                return blued::attribute_access_result::uuid_equal;
        }

        return blued::attribute_access_result::read_not_permitted;
    }

}

BOOST_AUTO_TEST_CASE( fitting_16bit )
{
    const auto             filter     = fixture< bluetoe::details::uuid16< 0x1234 > >();
    const blued::attribute attribute  = { 0x1234, nullptr };

    BOOST_CHECK( filter( 1,    attribute ) );
    BOOST_CHECK( filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( not_fitting_16bit )
{
    const auto             filter     = fixture< bluetoe::details::uuid16< 0x1234 > >();
    const blued::attribute attribute  = { 0x4711, nullptr };

    BOOST_CHECK( !filter( 1,    attribute ) );
    BOOST_CHECK( !filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( fitting_16bit_compaired_with_bluetooth_base_uuid )
{
    const auto             filter     = fixture< blued::bluetooth_base_uuid::from_16bit< 0x1234 > >();
    const blued::attribute attribute  = { 0x1234, nullptr };

    BOOST_CHECK( filter( 1,    attribute ) );
    BOOST_CHECK( filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( not_fitting_16bit_compaired_with_bluetooth_base_uuid )
{
    const auto             filter     = fixture< blued::bluetooth_base_uuid::from_16bit< 0x1234 > >();
    const blued::attribute attribute  = { 0x4711, nullptr };

    BOOST_CHECK( !filter( 1,    attribute ) );
    BOOST_CHECK( !filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( fitting_128bit )
{
    const auto             filter     = fixture< bluetoe::details::uuid< 0x1, 0x2, 0x3, 0x4, 0x5 > >();
    const blued::attribute attribute  = { bits( blued::gatt_uuids::internal_128bit_uuid ), equal_to_big_uuid };

    BOOST_CHECK( filter( 1,    attribute ) );
    BOOST_CHECK( filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( not_fitting_128bit )
{
    const auto             filter     = fixture< bluetoe::details::uuid< 0x1, 0x2, 0x3, 0x4, 0x6 > >();
    const blued::attribute attribute  = { bits( blued::gatt_uuids::internal_128bit_uuid ), equal_to_big_uuid };

    BOOST_CHECK( !filter( 1,    attribute ) );
    BOOST_CHECK( !filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( compare_16bit_with_128bit )
{
    const auto             filter     = fixture< bluetoe::details::uuid16< 0x1234 > >();
    const blued::attribute attribute  = { bits( blued::gatt_uuids::internal_128bit_uuid ), equal_to_big_uuid };

    BOOST_CHECK( !filter( 1,    attribute ) );
    BOOST_CHECK( !filter( 4711, attribute ) );
}

BOOST_AUTO_TEST_CASE( compare_128bit_with_16bit )
{
    const auto             filter     = fixture< bluetoe::details::uuid< 0x1, 0x2, 0x3, 0x4, 0x6 > >();
    const blued::attribute attribute  = { 0x4711, nullptr };

    BOOST_CHECK( !filter( 1,    attribute ) );
    BOOST_CHECK( !filter( 4711, attribute ) );
}


