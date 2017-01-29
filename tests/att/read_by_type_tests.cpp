#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"
#include "test_services.hpp"

BOOST_AUTO_TEST_SUITE( read_by_type_errors )

BOOST_FIXTURE_TEST_CASE( invalid_request_size, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03 }, 0x08, 0x0000, 0x04 ) );
    BOOST_CHECK( check_error_response( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x04, 0x05, 0x06 }, 0x08, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_zero, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x00, 0x00, 0xff, 0xff, 0x03, 0x28 }, 0x08, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( start_handle_greater_than_end_handle, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x05, 0x00, 0x04, 0x00, 0x03, 0x28 }, 0x08, 0x0005, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( handle_out_of_range, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x40, 0x00, 0xff, 0xff, 0x00, 0x28 }, 0x08, 0x0040, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( no_such_type, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x08, 0x01, 0x00, 0xff, 0xff, 0xab, 0xcd }, 0x08, 0x0001, 0x0A ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( read_by_type )

BOOST_FIXTURE_TEST_CASE( read_a_single_attribute, test::small_temperature_service_with_response<> )
{
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x02,                       // Characteristic Properties (read)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_FIXTURE_TEST_CASE( read_a_single_attribute_using_128bit_uuid, test::small_temperature_service_with_response<> )
{
    l2cap_input( {
        0x08, 0x01, 0x00, 0xff, 0xff,
        0xFB, 0x34, 0x9B, 0x5F,     // 16 bit UUID extended to 128 bit
        0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00,
        0x03, 0x28, 0x00, 0x00 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x02,                       // Characteristic Properties (read)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef test::request_with_reponse< test::three_apes_service, 100 > request_with_reponse_three_apes_service_100;

BOOST_FIXTURE_TEST_CASE( read_multiple_attributes, request_with_reponse_three_apes_service_100 )
{
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x04, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x05, 0x00,                 // Characteristic Value Handle
        0xAB, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x06, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x07, 0x00,                 // Characteristic Value Handle
        0xAC, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef test::request_with_reponse< test::three_apes_service, 50 > request_with_reponse_three_apes_service_50;

BOOST_FIXTURE_TEST_CASE( read_multiple_attributes_buffer_to_small, request_with_reponse_three_apes_service_50 )
{
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x04, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x05, 0x00,                 // Characteristic Value Handle
        0xAB, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_FIXTURE_TEST_CASE( read_multiple_attributes_buffer_in_range, request_with_reponse_three_apes_service_100 )
{
    l2cap_input( { 0x08, 0x01, 0x00, 0x04, 0x00, 0x03, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x04, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x05, 0x00,                 // Characteristic Value Handle
        0xAB, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< std::uint8_t, &test::ape1 >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x0815 >,
            bluetoe::bind_characteristic_value< std::uint8_t, &test::ape2 >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAC >,
            bluetoe::bind_characteristic_value< std::uint8_t, &test::ape3 >
        >
    >
> server_with_16bit_uuid_in_the_middle;

typedef test::request_with_reponse< server_with_16bit_uuid_in_the_middle, 200 > r_and_r_with_server_with_16bit_uuid_in_the_middle_100;

BOOST_FIXTURE_TEST_CASE( read_multiple_attributes_within_mixed_size, r_and_r_with_server_with_16bit_uuid_in_the_middle_100 )
{
    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x03, 0x28 } );

    static const std::uint8_t expected_result[] = {
        0x09, 0x15,                 // response code, size = 2 for handle and 19 for attribute value (Properties, Value Handle + UUID)
        0x02, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x03, 0x00,                 // Characteristic Value Handle
        0xAA, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x06, 0x00,                 // attribute handle
        0x0A,                       // Characteristic Properties (read + write)
        0x07, 0x00,                 // Characteristic Value Handle
        0xAC, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( &response[ 0 ], &response[ response_size ], std::begin( expected_result ), std::end( expected_result ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( read_by_type_seen_in_the_wild )

std::uint32_t temperature_value = 0x12345678;
static constexpr char server_name[] = "Temperature";
static constexpr char char_name[] = "Temperature Value";

typedef bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_name< char_name >,
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >
        >
    >
> small_temperature_service;

BOOST_FIXTURE_TEST_CASE( should_find_the_device_name_characteristic, test::request_with_reponse< small_temperature_service > )
{
    l2cap_input( { 0x02, 0x68, 0x00 } );
    expected_result( { 0x03, 0x17, 0x00 } );

    l2cap_input( { 0x08, 0x01, 0x00, 0xff, 0xff, 0x00, 0x2a } );
    BOOST_REQUIRE( response_size > 0 );
    BOOST_CHECK_EQUAL( response[ 0 ], 0x09 );
}

BOOST_AUTO_TEST_SUITE_END()
