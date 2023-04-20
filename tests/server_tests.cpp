#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include "test_servers.hpp"

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
    connection_data() : Server::connection_data() {}
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

BOOST_AUTO_TEST_SUITE( mixins )

std::string tag_construction_order;

struct reset_tag_construction_order
{
    reset_tag_construction_order()
    {
        tag_construction_order = "";
    }
};

template < char C >
struct tag
{
    tag()
    {
        tag_construction_order += C;
    }
};


BOOST_FIXTURE_TEST_CASE( not_mixin, reset_tag_construction_order )
{
    test::three_apes_service three_appes;
    BOOST_CHECK_EQUAL( tag_construction_order, "" );
}

BOOST_FIXTURE_TEST_CASE( empty_mixin, reset_tag_construction_order )
{
    bluetoe::server<
        bluetoe::mixin<>
    > server;

    BOOST_CHECK_EQUAL( tag_construction_order, "" );
}

BOOST_FIXTURE_TEST_CASE( server_mixins, reset_tag_construction_order )
{
    bluetoe::server<
        bluetoe::mixin< tag< 'A' > >
    > server;

    BOOST_CHECK_EQUAL( tag_construction_order, "A" );
}

BOOST_FIXTURE_TEST_CASE( multiple_server_mixins, reset_tag_construction_order )
{
    bluetoe::server<
        bluetoe::mixin<>,
        bluetoe::mixin< tag< 'A' > >,
        bluetoe::mixin< tag< 'B' >, tag< 'C' > >
    > server;

    BOOST_CHECK_EQUAL( tag_construction_order, "ABC" );
}

BOOST_FIXTURE_TEST_CASE( single_service_mixin, reset_tag_construction_order )
{
    bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x0815 >,
            bluetoe::mixin< tag< 'A' > >
        >
    > server;

    BOOST_CHECK_EQUAL( tag_construction_order, "A" );
}

BOOST_FIXTURE_TEST_CASE( multiple_service_mixin, reset_tag_construction_order )
{
    bluetoe::server<
        bluetoe::service<
            bluetoe::mixin< tag< 'A' > >,
            bluetoe::service_uuid16< 0x0815 >,
            bluetoe::mixin< tag< 'B' >, tag< 'C' > >
        >
    > server;

    BOOST_CHECK_EQUAL( tag_construction_order, "ABC" );
}

BOOST_FIXTURE_TEST_CASE( multiple_service_and_server_mixin, reset_tag_construction_order )
{
    bluetoe::server<
        bluetoe::mixin< tag< 'A' > >,
        bluetoe::service<
            bluetoe::mixin< tag< 'B' > >,
            bluetoe::service_uuid16< 0x0815 >,
            bluetoe::mixin< tag< 'C' >, tag< 'D' > >
        >,
        bluetoe::mixin< tag< 'E' > >,
        bluetoe::service<
            bluetoe::mixin< tag< 'F' > >,
            bluetoe::service_uuid16< 0x0815 >,
            bluetoe::mixin< tag< 'G' >, tag< 'H' > >
        >,
        bluetoe::mixin< tag< 'I' > >
    > server;

    BOOST_CHECK_EQUAL( tag_construction_order, "AEIBCDFGH" );
}

BOOST_AUTO_TEST_SUITE_END()

/*
 * Make sure, unauthorzed reads are reported as such and not just as read_not_permitted
 */

BOOST_AUTO_TEST_SUITE( unauthorized_reads_and_writes )

    char value = 0x42;

    using server_t = bluetoe::server<
        bluetoe::no_gap_service_for_gatt_servers,
        bluetoe::service<
            bluetoe::service_uuid16< 0x0815 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x0816 >,
                bluetoe::bind_characteristic_value< char, &value >,
                bluetoe::requires_encryption
            >
        >,
        bluetoe::shared_write_queue< 255 >
    >;

BOOST_FIXTURE_TEST_CASE( read_request, test::request_with_reponse< server_t > )
{
    static const std::uint8_t read[] = { 0x0A, 0x03, 0x00 };

    BOOST_CHECK( check_error_response( read, 0x0A, 0x0003, 0x05 ) );
}

BOOST_FIXTURE_TEST_CASE( read_blob_request, test::request_with_reponse< server_t > )
{
    static const std::uint8_t read[] = { 0x0C, 0x03, 0x00, 0x00, 0x00 };

    BOOST_CHECK( check_error_response( read, 0x0C, 0x0003, 0x05 ) );
}

BOOST_FIXTURE_TEST_CASE( read_multiple_request, test::request_with_reponse< server_t > )
{
    static const std::uint8_t read[] = { 0x0E, 0x03, 0x00, 0x03, 0x00 };

    BOOST_CHECK( check_error_response( read, 0x0E, 0x0003, 0x05 ) );
}

BOOST_FIXTURE_TEST_CASE( write_request, test::request_with_reponse< server_t > )
{
    static const std::uint8_t write[] = { 0x12, 0x03, 0x00, 0x00 };

    BOOST_CHECK( check_error_response( write, 0x12, 0x0003, 0x05 ) );
}

BOOST_FIXTURE_TEST_CASE( prepair_write_request, test::request_with_reponse< server_t > )
{
    static const std::uint8_t write[] = { 0x16, 0x03, 0x00, 0x00, 0x00, 0x00 };

    BOOST_CHECK( check_error_response( write, 0x16, 0x0003, 0x05 ) );
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE( cccd_updates )

    struct callback_t {
        template < class Server >
        void client_characteristic_configuration_updated( Server&, const bluetoe::details::client_characteristic_configuration& data )
        {
            ++cb_called;
            BOOST_CHECK_EQUAL( std::size_t{Server::number_of_client_configs}, 2u );

            notified0  = data.flags(0);
            indicated1 = data.flags(1);
        }

        int cb_called = 0;
        std::uint16_t notified0 = 0;
        std::uint16_t indicated1 = 0;
    } callback;

    struct reset_callback {
        reset_callback()
        {
            callback = callback_t();
        }
    };

    char value1 = 0x42;
    char value2 = 0x42;

    using server_t = bluetoe::server<
        bluetoe::no_gap_service_for_gatt_servers,
        bluetoe::service<
            bluetoe::service_uuid16< 0x0815 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x0816 >,
                bluetoe::bind_characteristic_value< char, &value1 >,
                bluetoe::no_encryption_required,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x0817 >,
                bluetoe::bind_characteristic_value< char, &value2 >,
                bluetoe::no_encryption_required,
                bluetoe::indicate
            >
        >,
        bluetoe::client_characteristic_configuration_update_callback< callback_t, callback >,
        bluetoe::mixin< reset_callback >
    >;

BOOST_FIXTURE_TEST_CASE( change_notification_subscription, test::request_with_reponse< server_t > )
{
    l2cap_input({ 0x12, 0x04, 0x00, 0x01, 0x00 });
    expected_result( { 0x13 });

    BOOST_CHECK_EQUAL( callback.cb_called, 1 );
    BOOST_CHECK_EQUAL( callback.notified0, 0x01 );
    BOOST_CHECK_EQUAL( callback.indicated1, 0x00 );
}

BOOST_FIXTURE_TEST_CASE( change_indication_subscription, test::request_with_reponse< server_t > )
{
    l2cap_input({ 0x12, 0x07, 0x00, 0x02, 0x00 });
    expected_result( { 0x13 });

    BOOST_CHECK_EQUAL( callback.cb_called, 1 );
    BOOST_CHECK_EQUAL( callback.notified0, 0x00 );
    BOOST_CHECK_EQUAL( callback.indicated1, 0x02 );
}

BOOST_FIXTURE_TEST_CASE( no_cb_call_on_none_changeing_write, test::request_with_reponse< server_t > )
{
    l2cap_input({ 0x12, 0x04, 0x00, 0x00, 0x00 });
    expected_result( { 0x13 });

    l2cap_input({ 0x12, 0x07, 0x00, 0x00, 0x00 });
    expected_result( { 0x13 });

    BOOST_CHECK_EQUAL( callback.cb_called, 0 );
    BOOST_CHECK_EQUAL( callback.notified0, 0x00 );
    BOOST_CHECK_EQUAL( callback.indicated1, 0x00 );
}

BOOST_AUTO_TEST_SUITE_END()
