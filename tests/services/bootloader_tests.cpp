
#include <bluetoe/server.hpp>
#include <bluetoe/services/bootloader.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_gatt.hpp"

using namespace test;

struct handler {
    std::pair< const std::uint8_t*, std::size_t > get_version()
    {
        static const std::uint8_t version[] = { 0x47, 0x11 };
        return std::pair< const std::uint8_t*, std::size_t >( version, sizeof( version ) );
    }
};

using bootloader_server = bluetoe::server<
    bluetoe::bootloader_service<
        bluetoe::bootloader::page_size< 1024 >,
        bluetoe::bootloader::page_align< 1024 >,
        bluetoe::bootloader::handler< handler >,
        bluetoe::bootloader::white_list<
            bluetoe::bootloader::memory_region< 0x1000, 0x2000 >
        >
    >
>;

BOOST_FIXTURE_TEST_CASE( service_discoverable_by_uuid, gatt_procedures< bootloader_server > )
{
    BOOST_CHECK_NE(
        ( discover_primary_service_by_uuid< bluetoe::bootloader::service_uuid >() ),
        discovered_service( 0, 0 ) );
}

template < class Server >
struct services_discovered : gatt_procedures< Server >
{
    services_discovered()
        : service( this->template discover_primary_service_by_uuid< bluetoe::bootloader::service_uuid >() )
    {
        BOOST_REQUIRE_NE( service, discovered_service() );
    }

    const discovered_service service;
};

BOOST_FIXTURE_TEST_CASE( characteristics_are_discoverable, services_discovered< bootloader_server > )
{
    BOOST_CHECK_NE(
        discover_characteristic_by_uuid< bluetoe::bootloader::control_point_uuid >( service ),
        discovered_characteristic() );

    BOOST_CHECK_NE(
        discover_characteristic_by_uuid< bluetoe::bootloader::data_uuid >( service ),
        discovered_characteristic() );
}

template < class Server >
struct all_discovered : services_discovered< Server >
{
    all_discovered()
        : cp_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::control_point_uuid >( this->service ) )
        , data_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::data_uuid >( this->service ) )
    {
        BOOST_REQUIRE_NE( cp_char, discovered_characteristic() );
        BOOST_REQUIRE_NE( data_char, discovered_characteristic() );
    }

    const discovered_characteristic cp_char;
    const discovered_characteristic data_char;
};

BOOST_FIXTURE_TEST_CASE( control_point_subscribe_for_notifications, all_discovered< bootloader_server > )
{
    const auto cccd = discover_cccd( cp_char );
    BOOST_CHECK_NE( cccd, discovered_characteristic_descriptor() );

    l2cap_input({
        0x12, low( cccd.handle ), high( cccd.handle ),
        0x01, 0x00
    });

    expected_result( { 0x13 } );
}

BOOST_FIXTURE_TEST_CASE( data_subscribe_for_notifications, all_discovered< bootloader_server > )
{
    const auto cccd = discover_cccd( data_char );
    BOOST_CHECK_NE( cccd, discovered_characteristic_descriptor() );

    l2cap_input({
        0x12, low( cccd.handle ), high( cccd.handle ),
        0x01, 0x00
    });

    expected_result( { 0x13 } );
}

template < class Server >
struct all_discovered_and_subscribed : all_discovered< Server >
{
    all_discovered_and_subscribed()
        : cp_cccd( this->discover_cccd( this->cp_char ) )
        , data_cccd( this->discover_cccd( this->data_char ) )
    {
        this->l2cap_input({
            0x12, this->low( cp_cccd.handle ), this->high( cp_cccd.handle ),
            0x01, 0x00
        });
        this->l2cap_input({
            0x12, this->low( data_cccd.handle ), this->high( data_cccd.handle ),
            0x01, 0x00
        });

    }

    void add_ptr( std::vector< std::uint8_t >& v, std::uintptr_t p )
    {
        for ( int i = 0; i != sizeof( p ); ++i )
        {
            v.push_back( p & 0xff );
            p = p >> 8;
        }
    }

    const discovered_characteristic_descriptor    cp_cccd;
    const discovered_characteristic_descriptor    data_cccd;
};

BOOST_FIXTURE_TEST_CASE( write_page_without_command, all_discovered_and_subscribed< bootloader_server > )
{
    check_error_response( {
        0x12, low( data_char.value_handle ), high( data_char.value_handle ),
        0x00, 0x01, 0x02, 0x03 },
        0x12, data_char.value_handle, 0x80 );
}

BOOST_FIXTURE_TEST_CASE( get_version, all_discovered_and_subscribed< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x00 }, connection );
    expected_result( { 0x13 } );

    expected_output( notification, {
        0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),    // indication
        0x01,                                                               // response code
        0x47, 0x11                                                          // version as given by handler::get_version()
    } );
}

BOOST_FIXTURE_TEST_CASE( flash_address_wrong_ptr_size, all_discovered_and_subscribed< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01, 0x12 }, connection );

    expected_result( {
        0x01, 0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x82 } ); // start_address_invalid

    // no notification expected
    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( flash_address_outside_of_white_list, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x2212 );

    l2cap_input( input, connection );

    expected_result( {
        0x01, 0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x83 } ); // start_address_not_accaptable

    // no notification expected
    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( flash_address, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x1212 );

    l2cap_input( input, connection );

    expected_result( { 0x13 } );

    // no notification expected
    BOOST_CHECK( !notification.valid() );
}

