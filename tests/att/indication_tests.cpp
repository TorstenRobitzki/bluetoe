#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include "../test_servers.hpp"

BOOST_AUTO_TEST_SUITE( indications )

    std::uint8_t value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value >,
                bluetoe::indicate
            >
        >
    > simple_server;

    BOOST_FIXTURE_TEST_CASE( l2cap_layer_gets_notified, request_with_reponse< simple_server > )
    {
        indicate( value );

        BOOST_CHECK( notification.valid() );
        BOOST_CHECK_EQUAL( notification.value_attribute().uuid, 0x8C8B );
        BOOST_CHECK_EQUAL( notification_type, simple_server::indication );
    }

BOOST_AUTO_TEST_SUITE_END()
