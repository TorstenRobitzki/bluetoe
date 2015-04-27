#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>

BOOST_AUTO_TEST_SUITE( characteristic_configuration )

std::uint16_t temperature_value1 = 0x0104;
std::uint16_t temperature_value2 = 0x0104;
std::uint16_t temperature_value3 = 0x0104;
std::uint16_t temperature_value4 = 0x0104;
std::uint16_t temperature_value5 = 0x0104;
std::uint16_t temperature_value6 = 0x0104;
std::uint16_t temperature_value7 = 0x0104;
std::uint16_t temperature_value8 = 0x0104;
std::uint16_t temperature_value9 = 0x0104;

template < std::uint16_t* P, std::uint16_t ID >
using temp_char = bluetoe::characteristic<
    bluetoe::characteristic_uuid16< ID >,
    bluetoe::bind_characteristic_value< std::uint16_t, P >,
    bluetoe::notify >;

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        temp_char< &temperature_value1, 1 >,
        temp_char< &temperature_value2, 2 >,
        temp_char< &temperature_value3, 3 >,
        temp_char< &temperature_value4, 4 >,
        temp_char< &temperature_value5, 5 >,
        temp_char< &temperature_value6, 6 >,
        temp_char< &temperature_value7, 7 >,
        temp_char< &temperature_value8, 8 >,
        temp_char< &temperature_value9, 9 >
    >
> large_temperature_service;

template < class Server = large_temperature_service >
struct connection_data : Server::connection_data
{
    connection_data() : Server::connection_data( 23 ) {}
};

BOOST_FIXTURE_TEST_CASE( characteristic_configuration_is_initialized_with_zereo, connection_data<> )
{
    BOOST_CHECK_EQUAL( 0, client_configurations().flags( 0 ) );
    BOOST_CHECK_EQUAL( 0, client_configurations().flags( 1 ) );
    // ...
    BOOST_CHECK_EQUAL( 0, client_configurations().flags( 8 ) );
}

BOOST_FIXTURE_TEST_CASE( is_writeable, connection_data<> )
{
    client_configurations().flags( 0, 0x3 );
    BOOST_CHECK_EQUAL( 0x3, client_configurations().flags( 0 ) );
}

BOOST_FIXTURE_TEST_CASE( configurations_are_independent, connection_data<> )
{
    client_configurations().flags( 0, 0x3 );
    client_configurations().flags( 1, 0x2 );
    client_configurations().flags( 2, 0x1 );
    client_configurations().flags( 3, 0x1 );
    client_configurations().flags( 4, 0x2 );
    client_configurations().flags( 5, 0x3 );
    BOOST_CHECK_EQUAL( 0x3, client_configurations().flags( 0 ) );
    BOOST_CHECK_EQUAL( 0x2, client_configurations().flags( 1 ) );
    BOOST_CHECK_EQUAL( 0x1, client_configurations().flags( 2 ) );
    BOOST_CHECK_EQUAL( 0x1, client_configurations().flags( 3 ) );
    BOOST_CHECK_EQUAL( 0x2, client_configurations().flags( 4 ) );
    BOOST_CHECK_EQUAL( 0x3, client_configurations().flags( 5 ) );
}

BOOST_FIXTURE_TEST_CASE( is_clearable, connection_data<> )
{
    client_configurations().flags( 7, 0x3 );
    client_configurations().flags( 7, 0x1 );
    BOOST_CHECK_EQUAL( 0x1, client_configurations().flags( 7 ) );
}

BOOST_AUTO_TEST_SUITE_END()
