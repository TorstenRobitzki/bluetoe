#include <iostream>
#include <bluetoe/service.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_attribute_access.hpp"
#include "hexdump.hpp"

namespace blued = bluetoe::details;

BOOST_AUTO_TEST_SUITE( implicit_characteristic_uuid )

    static std::int64_t x_pos;
    static std::int64_t y_pos;
    static std::int64_t z_pos;

    typedef bluetoe::service<
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< decltype( x_pos ), &x_pos >
        >,
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< decltype( y_pos ), &y_pos >
        >,
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< decltype( z_pos ), &z_pos >
        >,
        bluetoe::service_uuid< 0xD7E08435, 0xA713, 0x4A51, 0x92DB, 0x004A8C63B6F8 >
    > auto_uuid_service;

    BOOST_FIXTURE_TEST_CASE( check_correct_number_of_attributes, auto_uuid_service )
    {
        BOOST_CHECK_EQUAL( int( number_of_attributes ), 7u );
    }


    BOOST_FIXTURE_TEST_CASE( three_different_uuids_in_characteristic_value_declaration, auto_uuid_service )
    {
        static const std::array< std::uint8_t, 16u > service_uuid = { {
            0xf8, 0xb6, 0x63, 0x8c, 0x4a, 0x00, 0xdb, 0x92,
            0x51, 0x4a, 0x13, 0xa7, 0x35, 0x84, 0xe0, 0xd7
        } };

        static const std::array< int, 1 /*3*/ > handles = {{ 2 /*, 4, 6*/ }};

        for ( int handle_idx = 0; handle_idx != handles.size(); ++handle_idx )
        {
             // the expected UUID is the service uuid, with the lsb xored by the characteristic index
            auto expected_char_uuid = service_uuid;
            expected_char_uuid[ 0 ] ^= handle_idx +1;

            std::uint8_t uuid[ 16 ];

            auto read = blued::attribute_access_arguments::read( uuid, 3 );
            auto attr = attribute_at< 0 >( handles[ handle_idx ] - 1 );

            auto result = attr.access( read, handles[ handle_idx ] );
            BOOST_CHECK( result == blued::attribute_access_result::success );

            BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_char_uuid ), std::end( expected_char_uuid ), std::begin( uuid ), std::begin( uuid ) + read.buffer_size );
        }
    }

BOOST_AUTO_TEST_SUITE_END()
