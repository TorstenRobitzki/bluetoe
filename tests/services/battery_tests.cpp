#include <bluetoe/services/bas.hpp>
#include <bluetoe/server.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_gatt.hpp"
#include <iterator>

int battery_level = 14;

struct battery_handler {
    int read_battery_level()
    {
        return battery_level;
    }
};

using server_with_notification = bluetoe::server<
    bluetoe::battery_level<
        bluetoe::bas::handler< battery_handler >
    >,
    bluetoe::mixin< battery_handler >
>;

BOOST_FIXTURE_TEST_CASE( discover_service, test::gatt_procedures< server_with_notification > )
{
    const auto service = discover_primary_service_by_uuid< bluetoe::bas::service_uuid >();
    BOOST_CHECK( service.starting_handle != service.ending_handle );
}

BOOST_FIXTURE_TEST_CASE( discover_characteristic, test::gatt_procedures< server_with_notification > )
{
    const auto service = discover_primary_service_by_uuid< bluetoe::bas::service_uuid >();
    const auto chars   = discover_characteristic_by_uuid< bluetoe::bas::level_uuid >( service );

    BOOST_CHECK( !( chars == test::discovered_characteristic() ) );
}

struct discovered : test::gatt_procedures< server_with_notification >
{
    discovered()
    {
        battery_level   = 14;
        service         = discover_primary_service_by_uuid< bluetoe::bas::service_uuid >();
        characteristic  = discover_characteristic_by_uuid< bluetoe::bas::level_uuid >( service );
        cccd            = discover_cccd( characteristic );
    }

    test::discovered_service                    service;
    test::discovered_characteristic             characteristic;
    test::discovered_characteristic_descriptor  cccd;
};

BOOST_FIXTURE_TEST_CASE( check_level_value, discovered )
{
    l2cap_input({
        0x0A,                       // read request
        low( characteristic.value_handle ),
        high( characteristic.value_handle )
    });

    expected_result( {
        0x0B,                       // read response
        14
    });

    battery_level = 42;

    l2cap_input({
        0x0A,                       // read request
        low( characteristic.value_handle ),
        high( characteristic.value_handle )
    });

    expected_result( {
        0x0B,                       // read response
        42
    });
}

BOOST_FIXTURE_TEST_CASE( check_characteristic_properties, discovered )
{
    BOOST_CHECK_EQUAL(
        characteristic.properties,
        0x02                            // Read
      | 0x10                            // Notify
    );
}

BOOST_FIXTURE_TEST_CASE( check_cccd, discovered )
{
    BOOST_REQUIRE_NE( cccd.handle, test::invalid_handle );
}

struct discovered_and_subscribed : discovered
{
    discovered_and_subscribed()
    {
        l2cap_input({
            0x12,               //  Write Request
            low( cccd.handle ),
            high( cccd.handle ),
            0x01, 0x00          // notifications
        });

        expected_result( { 0x13 } );
    }
};

BOOST_FIXTURE_TEST_CASE( notifying_level, discovered_and_subscribed )
{
    battery_level = 33;

    notifiy_battery_level( *this );

    expected_output( notification, {
        0x1B,                       // Notification
        low( characteristic.value_handle ),
        high( characteristic.value_handle ),
        33
    });
}

using server_with_encryption = bluetoe::server<
    bluetoe::battery_level<
        bluetoe::bas::handler< battery_handler >,
        bluetoe::requires_encryption
    >,
    bluetoe::mixin< battery_handler >
>;

BOOST_FIXTURE_TEST_CASE( make_sure_other_service_args_are_forwarded, test::gatt_procedures< server_with_encryption > )
{
    const auto service = discover_primary_service_by_uuid< bluetoe::bas::service_uuid >();
    BOOST_CHECK( service.starting_handle != service.ending_handle );

    test::discovered_characteristic value_char = discover_characteristic_by_uuid< bluetoe::bas::level_uuid >( service );

    l2cap_input({
        0x0A,                       // read request
        low( value_char.value_handle ),
        high( value_char.value_handle )
    });

    expected_result( {
        0x01,                       // error response
        0x0A,                       // read request
        low( value_char.value_handle ),
        high( value_char.value_handle ),
        0x02                        // Read Not Permitted
    });
}
