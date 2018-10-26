#include <bluetoe/gap_service.hpp>
#include <bluetoe/server.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"


std::uint16_t nupsy = 0x0104;
static constexpr char nupsy_name[] = "Nupsy-Server";

typedef bluetoe::server<
    bluetoe::server_name< nupsy_name >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( nupsy ), &nupsy >,
            bluetoe::no_write_access
        >
    >
> nupsy_service;

// service is discoverable and contains at least two characteristics
BOOST_FIXTURE_TEST_CASE( service_is_discoverable_by_default, test::request_with_reponse< nupsy_service > )
{
    // Find By Type Value Request, 1, 0xffff, <<primary service>>, <<gap service>>
    l2cap_input( { 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x00, 0x18 } );
    expected_result( {
        0x07,
        0x04, 0x00, 0x08, 0x00
    } );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( nupsy ), &nupsy >,
            bluetoe::no_write_access
        >
    >,
    bluetoe::no_gap_service_for_gatt_servers
> without_gap_service_service;

BOOST_FIXTURE_TEST_CASE( no_service_no_cookies, test::request_with_reponse< without_gap_service_service > )
{
    // Find By Type Value Request, 1, 0xffff, <<primary service>>, <<gap service>>
    check_error_response(
        { 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x11, 0x47 },
        0x06, 0x0001, 0x0a
    );
}

BOOST_FIXTURE_TEST_CASE( appearance_is_mandatory, test::request_with_reponse< nupsy_service > )
{
    // Read by Type Request, 0x0001, 0xffff, 0x2A01
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x01, 0x2A } );
    expected_result( {
        0x09,
        0x04, 0x08, 0x00, 0x00, 0x00
    } );
}

BOOST_FIXTURE_TEST_CASE( no_service_no_appearance, test::request_with_reponse< without_gap_service_service > )
{
    // Read by Type Request, 0x0001, 0xffff, 0x2A01
    check_error_response(
        { 0x08, 0x01, 0x00, 0xff, 0xff, 0x01, 0x2A },
        0x08, 0x0001, 0x0a
    );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( nupsy ), &nupsy >,
            bluetoe::no_write_access
        >
    >,
    bluetoe::appearance::location_and_navigation_display_device
> location_and_navigation_display_device_server;

BOOST_FIXTURE_TEST_CASE( configured_appearance_is_used, test::request_with_reponse< location_and_navigation_display_device_server > )
{
    // Read by Type Request, 0x0001, 0xffff, 0x2A01
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x01, 0x2A } );
    expected_result( {
        0x09,
        0x04, 0x08, 0x00, 0x42, 0x14
    } );
}

BOOST_FIXTURE_TEST_CASE( device_name_is_mandatory, test::request_with_reponse< nupsy_service > )
{
    // Read by Type Request, 0x0001, 0xffff, 0x2A00
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x00, 0x2A } );
    expected_result( {
        0x09,
        0x0e, 0x06, 0x00, // length + handle
        // "Nupsy-Server"
        'N', 'u', 'p', 's', 'y', '-', 'S', 'e', 'r', 'v', 'e', 'r'
    } );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( nupsy ), &nupsy >,
            bluetoe::no_write_access
        >
    >
> no_name_service;

BOOST_FIXTURE_TEST_CASE( default_name, test::request_with_reponse< no_name_service > )
{
    // Read by Type Request, 0x0001, 0xffff, 0x2A00
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x00, 0x2A } );
    expected_result( {
        0x09,
        0x10, 0x06, 0x00, // length + handle
        // "Bluetoe-Server"
        'B', 'l', 'u', 'e', 't', 'o', 'e', '-', 'S', 'e', 'r', 'v', 'e', 'r'
    } );
}
