#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( read_by_type_errors )

BOOST_FIXTURE_TEST_CASE( pdu_to_small, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0A, 0x02 }, 0x0A, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_to_large, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0A, 0x02, 0x00, 0xab }, 0x0A, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( no_such_handle, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0A, 0x17, 0xAA }, 0x0A, 0xAA17, 0x01 ) );
    BOOST_CHECK( check_error_response( { 0x0A, 0x04, 0x00 }, 0x0A, 0x0004, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_handle, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0A, 0x00, 0x00 }, 0x0A, 0x0000, 0x01 ) );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( test::temperature_value ), &test::temperature_value >,
            bluetoe::no_read_access
        >
    >
> unreadable_server;


BOOST_FIXTURE_TEST_CASE( not_readable, test::request_with_reponse< unreadable_server > )
{
    BOOST_CHECK( check_error_response( { 0x0A, 0x03, 0x00 }, 0x0A, 0x0003, 0x02 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( read_requests )

BOOST_FIXTURE_TEST_CASE( do_i_have_to_wear_a_jacket_today, test::small_temperature_service_with_response<> )
{
    l2cap_input( { 0x0A, 0x03, 0x00 } );

    static const std::uint8_t expected_result[] = { 0x0B, 0x04, 0x01 };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

static std::uint8_t blob[ 100 ] = { 0 };

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( blob ), &blob >
        >
    >
> blob_server;

BOOST_FIXTURE_TEST_CASE( read_first_part_of_blob, test::request_with_reponse< blob_server > )
{
    l2cap_input( { 0x0A, 0x03, 0x00 } );

    static const std::uint8_t expected_result[] = {
        0x0B,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00 };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

static const char char_name_foo[] = "Foo";
static const char char_name_bar[] = "Bar";
static const std::uint8_t descriptor_data[] = { 0x08, 0x15 };

using server_with_fixed_handles = bluetoe::server<
    bluetoe::no_gap_service_for_gatt_servers,
    bluetoe::service<
        bluetoe::attribute_handle< 0x020 >,
        bluetoe::service_uuid16< 0x0815 >,

        // Characteristic with CCCD and Characteristic User Description without fixed CCCD
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1516 >,
            bluetoe::attribute_handles< 0x50, 0x52 >,
            bluetoe::fixed_uint8_value< 0x42 >,
            bluetoe::characteristic_name< char_name_foo >,
            bluetoe::notify
        >,
        // Characteristic with CCCD and Characteristic User Description and user descriptor with fixed CCCD
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1517 >,
            bluetoe::attribute_handles< 0x60, 0x62, 0x64 >,
            bluetoe::fixed_uint8_value< 0x43 >,
            bluetoe::characteristic_name< char_name_bar >,
            bluetoe::descriptor< 0x1722, descriptor_data, sizeof( descriptor_data ) >,
            bluetoe::notify
        >
    >
>;

BOOST_FIXTURE_TEST_CASE( read_characteristic_values, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input( { 0x0A, 0x52, 0x00 } );
    expected_result( { 0X0B, 0x42 } );

    l2cap_input( { 0x0A, 0x62, 0x00 } );
    expected_result( { 0X0B, 0x43 } );
}

BOOST_FIXTURE_TEST_CASE( read_characteristic_declarations, test::request_with_reponse< server_with_fixed_handles > )
{
    l2cap_input( { 0x0A, 0x50, 0x00 } );
    expected_result( { 0X0B, 0x12, 0x52, 0x00, 0x16, 0x15 } );

    l2cap_input( { 0x0A, 0x60, 0x00 } );
    expected_result( { 0X0B, 0x12, 0x62, 0x00, 0x17, 0x15 } );
}

BOOST_AUTO_TEST_SUITE_END()
