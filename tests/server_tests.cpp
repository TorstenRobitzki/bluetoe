#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>

BOOST_AUTO_TEST_SUITE( characteristic_configuration )

std::uint16_t temperature_value1 = 0x0104;
std::uint16_t temperature_value2 = 0x0104;

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value1 ), &temperature_value1 >,
            bluetoe::notify
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value2 ), &temperature_value2 >,
            bluetoe::notify
        >
    >
> small_temperature_service;

template < class Server = small_temperature_service >
struct connection_data : Server::connection_data
{
    connection_data() : Server::connection_data( 23 ) {}
};

BOOST_FIXTURE_TEST_CASE( characteristic_configuration_is_initialized_with_zereo, connection_data<> )
{
    BOOST_CHECK_EQUAL( 0, client_configurations()[ 0 ].flags() );
    BOOST_CHECK_EQUAL( 0, client_configurations()[ 1 ].flags() );
}

BOOST_FIXTURE_TEST_CASE( is_writeable, connection_data<> )
{
    client_configurations()[ 0 ].flags( 0x3 );
    BOOST_CHECK_EQUAL( 0x3, client_configurations()[ 0 ].flags() );
}

BOOST_FIXTURE_TEST_CASE( configurations_are_independent, connection_data<> )
{
    client_configurations()[ 0 ].flags( 0x3 );
    client_configurations()[ 1 ].flags( 0x2 );
    BOOST_CHECK_EQUAL( 0x3, client_configurations()[ 0 ].flags() );
    BOOST_CHECK_EQUAL( 0x2, client_configurations()[ 1 ].flags() );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value1 ), &temperature_value1 >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value2 ), &temperature_value2 >
        >
    >
> no_notification_service;

BOOST_FIXTURE_TEST_CASE( if_no_client_configuration_is_required_no_data_is_used, connection_data< no_notification_service > )
{
    BOOST_CHECK( client_configurations() == nullptr );
}

BOOST_AUTO_TEST_SUITE_END()
