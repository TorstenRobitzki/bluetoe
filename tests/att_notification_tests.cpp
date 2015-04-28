#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include <array>

BOOST_AUTO_TEST_SUITE( simple_notify )

    std::uint8_t value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value >,
                bluetoe::notify
            >
        >
    > simple_server;

    template < typename Server, std::size_t NumberOfConnections = 3 >
    struct l2cap_layer : Server
    {
        l2cap_layer()
        {
            instance       = this;
            notification   = nullptr;

            this->notification_callback( &l2cap_layer_notify_cb );
        }

        static void l2cap_layer_notify_cb( const void* item )
        {
            notification = item;
        }

        static l2cap_layer*                                                         instance;
        static std::array< typename Server::connection_data, NumberOfConnections >  connections;
        static const void*                                                          notification;
    };

    template < typename Server, std::size_t NumberOfConnections >
    l2cap_layer< Server, NumberOfConnections >* l2cap_layer< Server, NumberOfConnections >::instance = nullptr;

    template < typename Server, std::size_t NumberOfConnections >
    std::array< typename Server::connection_data, NumberOfConnections > l2cap_layer< Server, NumberOfConnections >::connections;

    template < typename Server, std::size_t NumberOfConnections >
    const void* l2cap_layer< Server, NumberOfConnections >::notification = nullptr;

    BOOST_FIXTURE_TEST_CASE( no_data_when_not_enabled, l2cap_layer< simple_server > )
    {
        notify( value );

        BOOST_CHECK_EQUAL( &value, notification );
    }

BOOST_AUTO_TEST_SUITE_END()
