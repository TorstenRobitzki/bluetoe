#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/services/bootloader.hpp>

using bootloader_server = bluetoe::server<
    bluetoe::bootloader_service<
    >
>;

BOOST_FIXTURE_TEST_CASE( service_discoverable, bootloader_server )
{

}
