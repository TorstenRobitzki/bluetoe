// delete me if you see me
#include <iostream>

#include <bluetoe/attribute_handle.hpp>
#include <bluetoe/server.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

#include "test_servers.hpp"
#include "test_attribute_access.hpp"

using server_with_single_fixed_service = bluetoe::server<
    bluetoe::no_gap_service_for_gatt_servers,
    bluetoe::service<
        bluetoe::attribute_handle< 0x0100 >,
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_single_fixed_service, bluetoe::details::handle_index_mapping< server_with_single_fixed_service > )

    BOOST_AUTO_TEST_CASE( service_handle )
    {
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x0100u );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_single_not_fixed_service = bluetoe::server<
    bluetoe::no_gap_service_for_gatt_servers,
    bluetoe::service<
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_single_service, bluetoe::details::handle_index_mapping< server_with_single_not_fixed_service > )

    BOOST_AUTO_TEST_CASE( service_handle )
    {
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 1u );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_gap_service_service = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_single_service_with_gap, bluetoe::details::handle_index_mapping< server_with_gap_service_service > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // service with single characteristic == 3 handles
        // GAP Service contains 2 characteristics  == 5 handles
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 1u );
        BOOST_CHECK_EQUAL( handle_by_index( 1 ), 2u );
        BOOST_CHECK_EQUAL( handle_by_index( 2 ), 3u );
        BOOST_CHECK_EQUAL( handle_by_index( 3 ), 4u );
        BOOST_CHECK_EQUAL( handle_by_index( 4 ), 5u );
        BOOST_CHECK_EQUAL( handle_by_index( 5 ), 6u );
        BOOST_CHECK_EQUAL( handle_by_index( 6 ), 7u );
        BOOST_CHECK_EQUAL( handle_by_index( 7 ), 8u );
        BOOST_CHECK_EQUAL( handle_by_index( 8 ), bluetoe::details::invalid_attribute_handle );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_gap_and_one_fixed_service = bluetoe::server<
    bluetoe::service<
        bluetoe::attribute_handle< 0x010 >,
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_with_gap_and_one_fixed_service, bluetoe::details::handle_index_mapping< server_with_gap_and_one_fixed_service > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // service with single characteristic == 3 handles
        // GAP Service contains 2 characteristics  == 5 handles
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x010u );
        BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x011u );
        BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x012u );
        BOOST_CHECK_EQUAL( handle_by_index( 3 ), 0x013u );
        BOOST_CHECK_EQUAL( handle_by_index( 4 ), 0x014u );
        BOOST_CHECK_EQUAL( handle_by_index( 5 ), 0x015u );
        BOOST_CHECK_EQUAL( handle_by_index( 6 ), 0x016u );
        BOOST_CHECK_EQUAL( handle_by_index( 7 ), 0x017u );
        BOOST_CHECK_EQUAL( handle_by_index( 8 ), bluetoe::details::invalid_attribute_handle );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_partial_fixed_services = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x0816 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >,
    bluetoe::service<
        bluetoe::attribute_handle< 0x010 >,
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_with_partial_fixed_services, bluetoe::details::handle_index_mapping< server_with_partial_fixed_services > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // 2 service with single characteristic == 6 handles
        // GAP Service contains 2 characteristics  == 5 handles
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x001u );
        BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x002u );
        BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x003u );
        BOOST_CHECK_EQUAL( handle_by_index( 3 ), 0x010u );
        BOOST_CHECK_EQUAL( handle_by_index( 4 ), 0x011u );
        BOOST_CHECK_EQUAL( handle_by_index( 5 ), 0x012u );
        BOOST_CHECK_EQUAL( handle_by_index( 6 ), 0x013u );
        BOOST_CHECK_EQUAL( handle_by_index( 7 ), 0x014u );
        BOOST_CHECK_EQUAL( handle_by_index( 8 ), 0x015u );
        BOOST_CHECK_EQUAL( handle_by_index( 9 ), 0x016u );
        BOOST_CHECK_EQUAL( handle_by_index( 10 ), 0x017u );
        BOOST_CHECK_EQUAL( handle_by_index( 11 ), bluetoe::details::invalid_attribute_handle );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_multiple_fixed_services = bluetoe::server<
    bluetoe::service<
        bluetoe::attribute_handle< 0x020 >,
        bluetoe::service_uuid16< 0x0816 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >,
    bluetoe::service<
        bluetoe::attribute_handle< 0x100 >,
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_with_multiple_fixed_services, bluetoe::details::handle_index_mapping< server_with_multiple_fixed_services > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // 2 service with single characteristic == 6 handles
        // GAP Service contains 2 characteristics  == 5 handles
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x020u );
        BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x021u );
        BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x022u );
        BOOST_CHECK_EQUAL( handle_by_index( 3 ), 0x100u );
        BOOST_CHECK_EQUAL( handle_by_index( 4 ), 0x101u );
        BOOST_CHECK_EQUAL( handle_by_index( 5 ), 0x102u );
        BOOST_CHECK_EQUAL( handle_by_index( 6 ), 0x103u );
        BOOST_CHECK_EQUAL( handle_by_index( 7 ), 0x104u );
        BOOST_CHECK_EQUAL( handle_by_index( 8 ), 0x105u );
        BOOST_CHECK_EQUAL( handle_by_index( 9 ), 0x106u );
        BOOST_CHECK_EQUAL( handle_by_index( 10 ), 0x107u );
        BOOST_CHECK_EQUAL( handle_by_index( 11 ), bluetoe::details::invalid_attribute_handle );
    }

BOOST_AUTO_TEST_SUITE_END()

