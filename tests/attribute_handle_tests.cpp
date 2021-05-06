#include <bluetoe/attribute_handle.hpp>
#include <bluetoe/server.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

#include "test_servers.hpp"
#include "test_attribute_access.hpp"
#include "attribute_io.hpp"

template < typename Server >
struct fixture
{
    std::uint16_t handle_by_index( std::size_t index )
    {
        return bluetoe::details::handle_index_mapping< Server >::handle_by_index( index );
    }

    std::size_t index_by_handle( std::uint16_t handle )
    {
        return bluetoe::details::handle_index_mapping< Server >::index_by_handle( handle );
    }

    static std::size_t first_index_by_handle( std::uint16_t handle )
    {
        return bluetoe::details::handle_index_mapping< Server >::first_index_by_handle( handle );
    }

    void check_attribute( std::size_t index, std::initializer_list< std::uint8_t > value )
    {
        std::uint8_t value_buffer[ 100 ];
        std::uint8_t cccd_data[ 10 ] = { 0 };
        bluetoe::details::client_characteristic_configuration config( cccd_data, sizeof( cccd_data ) );
        Server srv;
        auto access = bluetoe::details::attribute_access_arguments::read( value_buffer, 0, config );

        const auto result = srv.attribute_at( index ).access( access, index );
        BOOST_CHECK_EQUAL( result, bluetoe::details::attribute_access_result::success );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            value.begin(), value.end(),
            &value_buffer[ 0 ], &value_buffer[ access.buffer_size ] );
    }
};

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


BOOST_FIXTURE_TEST_SUITE( mapping_single_fixed_service, fixture< server_with_single_fixed_service > )

    BOOST_AUTO_TEST_CASE( service_handle )
    {
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x0100u );
        BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x0101u );
        BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x0102u );
        BOOST_CHECK_EQUAL( handle_by_index( 3 ), bluetoe::details::invalid_attribute_handle );
    }

    BOOST_AUTO_TEST_CASE( attribute_values )
    {
        check_attribute( 0, {
            0x15, 0x08              // 0x0815 service UUID
        } );
        check_attribute( 1, {
            0x02,                   // Properties: Read
            0x02, 0x01,             // Characteristic Value Attribute Handle: 0x0102
            0x15, 0x08
        } );
    }

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x010 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x100 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x101 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x102 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x103 ), bluetoe::details::invalid_attribute_index );
    }

    BOOST_AUTO_TEST_CASE( invalid_handles )
    {
        BOOST_CHECK_EQUAL( index_by_handle( 1 ), bluetoe::details::invalid_attribute_index );
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

BOOST_FIXTURE_TEST_SUITE( mapping_single_service, fixture< server_with_single_not_fixed_service > )

    BOOST_AUTO_TEST_CASE( service_handle )
    {
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 1u );
    }

    BOOST_AUTO_TEST_CASE( attribute_values )
    {
        check_attribute( 0, {
            0x15, 0x08              // 0x0815 service UUID
        } );
        check_attribute( 1, {
            0x02,                   // Properties: Read
            0x03, 0x00,             // Characteristic Value Attribute Handle: 0x0003
            0x15, 0x08
        } );
    }

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x00 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x01 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x02 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x03 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x04 ), bluetoe::details::invalid_attribute_index );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x05 ), bluetoe::details::invalid_attribute_index );
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

BOOST_FIXTURE_TEST_SUITE( mapping_single_service_with_gap, fixture< server_with_gap_service_service > )

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

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x00 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x01 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x02 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x03 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x04 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x05 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x06 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x07 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x08 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x09 ), bluetoe::details::invalid_attribute_index );
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

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x0F ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x10 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x11 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x12 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x13 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x14 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x15 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x16 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x17 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x18 ), bluetoe::details::invalid_attribute_index );
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

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x000 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x001 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x002 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x003 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x004 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x010 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x011 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x012 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x013 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x014 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x015 ), 8u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x016 ), 9u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x017 ), 10u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x018 ), bluetoe::details::invalid_attribute_index );
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

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x000 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x020 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x021 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x022 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x023 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x100 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x101 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x102 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x103 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x104 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x105 ), 8u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x106 ), 9u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x107 ), 10u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x108 ), bluetoe::details::invalid_attribute_index );
    }

BOOST_AUTO_TEST_SUITE_END()

using server_with_multiple_fixed_services_and_characteristics = bluetoe::server<
    bluetoe::service<
        bluetoe::attribute_handle< 0x020 >,
        bluetoe::service_uuid16< 0x0816 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >,
        bluetoe::characteristic<
            bluetoe::attribute_handle< 0x100 >,
            bluetoe::fixed_uint8_value< 0x42 >,
            bluetoe::notify
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::attribute_handle< 0x200 >,
            bluetoe::fixed_uint8_value< 0x42 >,
            bluetoe::notify
        >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x42 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_with_multiple_fixed_services_and_characteristics, bluetoe::details::handle_index_mapping< server_with_multiple_fixed_services_and_characteristics > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // first service
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x020u );
            // 2 Characteristics
            BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x021u );
            BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x022u );

            BOOST_CHECK_EQUAL( handle_by_index( 3 ), 0x100u );
            BOOST_CHECK_EQUAL( handle_by_index( 4 ), 0x101u );
            BOOST_CHECK_EQUAL( handle_by_index( 5 ), 0x102u );

        // second service
        BOOST_CHECK_EQUAL( handle_by_index( 6 ), 0x103u );
            // 2 Characteristics
            BOOST_CHECK_EQUAL( handle_by_index( 7 ), 0x200u );
            BOOST_CHECK_EQUAL( handle_by_index( 8 ), 0x201u );
            BOOST_CHECK_EQUAL( handle_by_index( 9 ), 0x202u );

            BOOST_CHECK_EQUAL( handle_by_index( 10 ), 0x203u );
            BOOST_CHECK_EQUAL( handle_by_index( 11 ), 0x204u );

        // GAP Service
        BOOST_CHECK_EQUAL( handle_by_index( 12 ), 0x205u );
            // 2 Characteristics
            BOOST_CHECK_EQUAL( handle_by_index( 13 ), 0x206u );
            BOOST_CHECK_EQUAL( handle_by_index( 14 ), 0x207u );

            BOOST_CHECK_EQUAL( handle_by_index( 15 ), 0x208u );
            BOOST_CHECK_EQUAL( handle_by_index( 16 ), 0x209u );

        BOOST_CHECK_EQUAL( handle_by_index( 17 ), bluetoe::details::invalid_attribute_handle );
    }

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x01f ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x020 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x021 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x022 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x023 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x100 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x101 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x102 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x103 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x104 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x200 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x201 ), 8u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x202 ), 9u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x203 ), 10u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x204 ), 11u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x205 ), 12u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x206 ), 13u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x207 ), 14u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x208 ), 15u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x209 ), 16u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x20a ), bluetoe::details::invalid_attribute_index );
    }

BOOST_AUTO_TEST_SUITE_END()


using server_with_multiple_fixed_attributes_handles = bluetoe::server<
    bluetoe::no_gap_service_for_gatt_servers,
    bluetoe::service<
        bluetoe::attribute_handle< 0x020 >,
        bluetoe::service_uuid16< 0x0816 >,

        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x50, 0x52 >,
            bluetoe::fixed_uint8_value< 0x42 >
        >,
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x60, 0x62, 0x64 >,
            bluetoe::fixed_uint8_value< 0x43 >,
            bluetoe::notify
        >
    >,
    bluetoe::service<
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x44 >,
            bluetoe::notify
        >,
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x100, 0x101 >,
            bluetoe::fixed_uint8_value< 0x45 >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_with_multiple_fixed_attributes_handles, fixture< server_with_multiple_fixed_attributes_handles > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // first service
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x020u );
            // 2 Characteristics
            BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x050u );
            BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x052u );

            BOOST_CHECK_EQUAL( handle_by_index( 3 ), 0x060u );
            BOOST_CHECK_EQUAL( handle_by_index( 4 ), 0x062u );
            BOOST_CHECK_EQUAL( handle_by_index( 5 ), 0x064u );

        // second service
        BOOST_CHECK_EQUAL( handle_by_index( 6 ), 0x065u );
            // 2 Characteristics
            BOOST_CHECK_EQUAL( handle_by_index( 7 ), 0x066u );
            BOOST_CHECK_EQUAL( handle_by_index( 8 ), 0x067u );
            BOOST_CHECK_EQUAL( handle_by_index( 9 ), 0x068u );

            BOOST_CHECK_EQUAL( handle_by_index( 10 ), 0x100u );
            BOOST_CHECK_EQUAL( handle_by_index( 11 ), 0x101u );

        BOOST_CHECK_EQUAL( handle_by_index( 12 ), bluetoe::details::invalid_attribute_handle );
    }

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x01f ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x020 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x021 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x050 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x051 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x052 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x053 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x060 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x061 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x062 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x063 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x064 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x065 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x066 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x067 ), 8u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x068 ), 9u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x069 ), 10u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x100 ), 10u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x101 ), 11u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x102 ), bluetoe::details::invalid_attribute_index );
    }

    BOOST_AUTO_TEST_CASE( attribute_values )
    {
        // first service
        check_attribute( 0, {
            0x16, 0x08              // 0x0815 service UUID
        } );

        // first characteristic
        check_attribute( 1, {
            0x02,                   // Properties: Read
            0x52, 0x00,             // Characteristic Value Attribute Handle: 0x0052
            0x16, 0x08
        } );
        check_attribute( 2, { 0x42 } ); // Value

        // second characteristic
        check_attribute( 3, {
            0x12,                   // Properties: Read, Notify
            0x62, 0x00,             // Characteristic Value Attribute Handle: 0x0062
            0x16, 0x08
        } );
        check_attribute( 4, { 0x43 } ); // Value
        check_attribute( 5, { 0x00, 0x00 } ); // CCCD configuration

        // second service
        check_attribute( 6, {
            0x15, 0x08              // 0x0815 service UUID
        } );

        // first characteristic
        check_attribute( 7, {
            0x12,                   // Properties: Read, Notify
            0x67, 0x00,             // Characteristic Value Attribute Handle: 0x0067
            0x15, 0x08
        } );
        check_attribute( 8, { 0x44 } ); // Value
        check_attribute( 9, { 0x00, 0x00 } ); // CCCD configuration

        // second characteristic
        check_attribute( 10, {
            0x02,                   // Properties: Read
            0x01, 0x01,             // Characteristic Value Attribute Handle: 0x0101
            0x15, 0x08
        } );
        check_attribute( 11, { 0x45 } ); // Value
    }

BOOST_AUTO_TEST_SUITE_END()

static const char char_name[] = "Foo";
static const std::uint8_t descriptor_data[] = { 0x08, 0x15 };

using server_with_additional_descriptors = bluetoe::server<
    bluetoe::no_gap_service_for_gatt_servers,
    bluetoe::service<
        bluetoe::attribute_handle< 0x020 >,
        bluetoe::service_uuid16< 0x0815 >,

        // Characteristic with CCCD and Characteristic User Description without fixed CCCD
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x50, 0x52 >,
            bluetoe::fixed_uint8_value< 0x42 >,
            bluetoe::characteristic_name< char_name >,
            bluetoe::notify
        >,
        // Characteristic with CCCD and Characteristic User Description and user descriptor with fixed CCCD
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x60, 0x62, 0x64 >,
            bluetoe::fixed_uint8_value< 0x43 >,
            bluetoe::characteristic_name< char_name >,
            bluetoe::descriptor< 0x1722, descriptor_data, sizeof( descriptor_data ) >,
            bluetoe::notify
        >,
        // No descriptors, no fixup
        bluetoe::characteristic<
            bluetoe::fixed_uint8_value< 0x44 >
        >,
        // Characteristic Characteristic User Description without fixed CCCD
        bluetoe::characteristic<
            bluetoe::attribute_handles< 0x70, 0x80 >,
            bluetoe::fixed_uint8_value< 0x45 >,
            bluetoe::characteristic_name< char_name >
        >
    >
>;

BOOST_FIXTURE_TEST_SUITE( mapping_server_with_additional_descriptors, fixture< server_with_additional_descriptors > )

    BOOST_AUTO_TEST_CASE( all_handles )
    {
        // first service
        BOOST_CHECK_EQUAL( handle_by_index( 0 ), 0x020u );
            // Declaration, Value, CCCD, User Description
            BOOST_CHECK_EQUAL( handle_by_index( 1 ), 0x050u );
            BOOST_CHECK_EQUAL( handle_by_index( 2 ), 0x052u );
            BOOST_CHECK_EQUAL( handle_by_index( 3 ), 0x053u );
            BOOST_CHECK_EQUAL( handle_by_index( 4 ), 0x054u );

            // Declaration, Value, CCCD, User Description
            BOOST_CHECK_EQUAL( handle_by_index( 5 ), 0x060u );
            BOOST_CHECK_EQUAL( handle_by_index( 6 ), 0x062u );
            BOOST_CHECK_EQUAL( handle_by_index( 7 ), 0x064u );
            BOOST_CHECK_EQUAL( handle_by_index( 8 ), 0x065u );
            BOOST_CHECK_EQUAL( handle_by_index( 9 ), 0x066u );

            // Declaration, Value without fixup
            BOOST_CHECK_EQUAL( handle_by_index( 10 ), 0x067u );
            BOOST_CHECK_EQUAL( handle_by_index( 11 ), 0x068u );

            // Declaration, Value, User Description
            BOOST_CHECK_EQUAL( handle_by_index( 12 ), 0x070u );
            BOOST_CHECK_EQUAL( handle_by_index( 13 ), 0x080u );
            BOOST_CHECK_EQUAL( handle_by_index( 14 ), 0x081u );

        BOOST_CHECK_EQUAL( handle_by_index( 15 ), bluetoe::details::invalid_attribute_handle );
    }

    BOOST_AUTO_TEST_CASE( handle_to_index )
    {
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x01f ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x020 ), 0u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x021 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x050 ), 1u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x051 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x052 ), 2u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x053 ), 3u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x054 ), 4u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x055 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x060 ), 5u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x061 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x062 ), 6u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x063 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x064 ), 7u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x065 ), 8u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x066 ), 9u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x067 ), 10u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x068 ), 11u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x069 ), 12u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x070 ), 12u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x071 ), 13u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x07F ), 13u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x080 ), 13u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x081 ), 14u );
        BOOST_CHECK_EQUAL( first_index_by_handle( 0x082 ), bluetoe::details::invalid_attribute_index );
    }

    BOOST_AUTO_TEST_CASE( attribute_values )
    {
#if 0
        // first service
        check_attribute( 0, {
            0x15, 0x08              // 0x0815 service UUID
        } );

        // first characteristic
        check_attribute( 1, {
            0x12,                   // Properties: Read, Notify
            0x52, 0x00,             // Characteristic Value Attribute Handle: 0x0052
            0x15, 0x08
        } );
        check_attribute( 2, { 0x42 } ); // Value
        check_attribute( 3, { 0x00, 0x00 } ); // CCCD
        check_attribute( 4, { 'F', 'o', 'o' } ); // Characteristic User Description
        // second characteristic
        check_attribute( 1, {
            0x02,                   // Properties: Read
            0x52, 0x00,             // Characteristic Value Attribute Handle: 0x0052
            0x16, 0x08
        } );
        check_attribute( 2, { 0x42 } ); // Value

#endif
    }


BOOST_AUTO_TEST_SUITE_END()
