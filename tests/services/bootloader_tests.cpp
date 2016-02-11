
#include <bluetoe/server.hpp>
#include <bluetoe/services/bootloader.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_gatt.hpp"

using namespace test;

using bootloader_server = bluetoe::server<
    bluetoe::bootloader_service<
        bluetoe::bootloader::page_size< 1024 >,
        bluetoe::bootloader::page_align< 1024 >
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
        : cp_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::control_point_uuid >() )
        , data_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::data_uuid >() )
    {
        BOOST_REQUIRE_NE( cp_char, discovered_characteristic() );
        BOOST_REQUIRE_NE( data_char, discovered_characteristic() );
    }

    const discovered_characteristic cp_char;
    const discovered_characteristic data_char;
};
