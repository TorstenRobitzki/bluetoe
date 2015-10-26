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

/* device name is mandatory */
BOOST_FIXTURE_TEST_CASE( named_server, request_with_reponse< nupsy_service > )
{
    l2cap_input( { 0x0C, 0x03, 0x00, 0x0A, 0x00 } );
//     expected_result( {
//         0x0D,
//         0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
//         0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
//         0x30, 0x31
//     } );
}
