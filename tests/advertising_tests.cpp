#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

static const char name[] = "Test Name";

typedef bluetoe::extend_server<
    small_temperature_service,
    bluetoe::server_name< name >
> named_temperature_service;

BOOST_AUTO_TEST_CASE( named_server )
{
    std::uint8_t buffer[ 100 ];
    named_temperature_service service;

    static const std::uint8_t expected_advertising[] = {
        0x02, 0x01, 0x06,
        0x0A, 0x09, 'T', 'e', 's', 't', ' ', 'N', 'a', 'm', 'e'
    };

    const std::size_t size = service.advertising_data( buffer, sizeof( buffer ) );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + size, std::begin( expected_advertising ), std::end( expected_advertising ) );
}

BOOST_AUTO_TEST_CASE( short_named_server )
{
    std::uint8_t buffer[ 10 ];
    named_temperature_service service;

    static const std::uint8_t expected_advertising[] = {
        0x02, 0x01, 0x06,
        0x06, 0x08, 'T', 'e', 's', 't', ' '
    };

    const std::size_t size = service.advertising_data( buffer, sizeof( buffer ) );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + size, std::begin( expected_advertising ), std::end( expected_advertising ) );
}

BOOST_AUTO_TEST_CASE( unnamed_server )
{
    std::uint8_t buffer[ 31 ];
    small_temperature_service service;

    static const std::uint8_t expected_advertising[] = {
        0x02, 0x01, 0x06
    };

    const std::size_t size = service.advertising_data( buffer, sizeof( buffer ) );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + size, std::begin( expected_advertising ), std::end( expected_advertising ) );
}
