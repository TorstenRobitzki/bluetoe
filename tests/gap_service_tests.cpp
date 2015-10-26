#include <bluetoe/gap_service.hpp>
#include <bluetoe/server.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"


std::uint16_t nupsy = 0x0104;

typedef bluetoe::server<
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
BOOST_FIXTURE_TEST_CASE( service_is_discoverable_by_default, request_with_reponse< nupsy_service > )
{
    // Find By Type Value Request, 1, 0xffff, <<primary service>>, <<gap service>>
    l2cap_input( { 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x11, 0x47 } );
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

BOOST_FIXTURE_TEST_CASE( no_service_no_cookies, request_with_reponse< without_gap_service_service > )
{
    // Find By Type Value Request, 1, 0xffff, <<primary service>>, <<gap service>>
    check_error_response(
        { 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x11, 0x47 },
        0x06, 0x0001, 0x0a
    );
}
