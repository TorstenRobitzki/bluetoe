#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer.hpp>

#include "connected.hpp"

std::uint32_t temperature_value = 0;

using notify_server = bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAB >,
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access,
                bluetoe::indicate
            >
        >,
        bluetoe::no_gap_service_for_gatt_servers
    >;

using link_layer = unconnected_base_t< notify_server, test::radio >;

BOOST_FIXTURE_TEST_CASE( request_cancelationen_when_notification_added, link_layer )
{
    const bool result = queue_lcap_notification(
        bluetoe::details::notification_data( 2, 0 ), this, bluetoe::details::notification_type::notification );

    BOOST_CHECK( result );
    BOOST_CHECK( event_cancelation_requested() );
}

BOOST_FIXTURE_TEST_CASE( request_cancelationen_when_indication_added, link_layer )
{
    const bool result = queue_lcap_notification(
        bluetoe::details::notification_data( 4, 1 ), this, bluetoe::details::notification_type::indication );

    BOOST_CHECK( result );
    BOOST_CHECK( event_cancelation_requested() );
}

BOOST_FIXTURE_TEST_CASE( no_request_when_notification_already_queued, link_layer )
{
    const bool result = queue_lcap_notification(
        bluetoe::details::notification_data( 2, 0 ), this, bluetoe::details::notification_type::notification );

    BOOST_CHECK( result );
    BOOST_CHECK( event_cancelation_requested() );

    BOOST_CHECK( !queue_lcap_notification(
        bluetoe::details::notification_data( 2, 0 ), this, bluetoe::details::notification_type::notification ) );
    BOOST_CHECK( !event_cancelation_requested() );
}

BOOST_FIXTURE_TEST_CASE( no_request_when_indication_already_queued, link_layer )
{
    const bool result = queue_lcap_notification(
        bluetoe::details::notification_data( 4, 1 ), this, bluetoe::details::notification_type::indication );

    BOOST_CHECK( result );
    BOOST_CHECK( event_cancelation_requested() );

    BOOST_CHECK( !queue_lcap_notification(
        bluetoe::details::notification_data( 4, 1 ), this, bluetoe::details::notification_type::indication ) );
    BOOST_CHECK( !event_cancelation_requested() );
}

BOOST_FIXTURE_TEST_CASE( no_request_when_confirmation_received, link_layer )
{
    const bool result = queue_lcap_notification(
        bluetoe::details::notification_data( 2, 0 ), this, bluetoe::details::notification_type::confirmation );

    BOOST_CHECK( result );
    BOOST_CHECK( !event_cancelation_requested() );

}

