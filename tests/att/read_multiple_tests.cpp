#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( read_multiple_errors )

BOOST_FIXTURE_TEST_CASE( pdu_to_small, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00 }, 0x0E, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_half_an_handle, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x03, 0x00, 0x04 }, 0x0E, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( the_first_handle_is_invalid, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00 }, 0x0E, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( the_second_handle_is_invalid, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00 }, 0x0E, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( the_first_handle_is_unknown, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x80, 0x03, 0x00, 0x04, 0x00 }, 0x0E, 0x8002, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( the_second_handle_is_unknown, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x03, 0x00, 0xf4, 0xff }, 0x0E, 0xfff4, 0x0A ) );
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


BOOST_FIXTURE_TEST_CASE( first_attribute_not_readable, test::request_with_reponse< unreadable_server > )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x03, 0x00, 0x02, 0x00 }, 0x0E, 0x0003, 0x02 ) );
}

BOOST_FIXTURE_TEST_CASE( last_attribute_not_readable, test::request_with_reponse< unreadable_server > )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x03, 0x00 }, 0x0E, 0x0003, 0x02 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( read_multiple )

BOOST_FIXTURE_TEST_CASE( read_two_attributes, test::small_temperature_service_with_response< > )
{
    l2cap_input( { 0x0E, 0x02, 0x00, 0x03, 0x00 } );

    expected_result( {
        0x0F,                                           // opcode
        0x02, 0x03, 0x00,                               // Characteristic Declaration
        0xAA, 0x3C, 0xC7, 0x5B, 0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D, 0x94, 0x40, 0x8B, 0x8C,
        0x04, 0x01                                      // Characteristic Value Declaration
    } );
}

BOOST_FIXTURE_TEST_CASE( read_all_attributes_last_attribute_clipped, test::small_temperature_service_with_response< > )
{
    l2cap_input( { 0x0E, 0x02, 0x00, 0x03, 0x00, 0x01, 0x00 } );
    expected_result( {
        0x0F,                                           // opcode
        0x02, 0x03, 0x00,                               // Characteristic Declaration
        0xAA, 0x3C, 0xC7, 0x5B, 0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D, 0x94, 0x40, 0x8B, 0x8C,
        0x04, 0x01,                                     // Characteristic Value Declaration
        0xA9                                            // Primary Service, clipped at the mtu of 23
    } );
}

// if the mtu is small enough that already the last but one attributes gets clipped, there should be a valid response
BOOST_FIXTURE_TEST_CASE( already_the_last_but_one_attribute_gets_clipped, test::request_with_reponse< test::three_apes_service > )
{
    // read the Primary Service, the first and second Characteristic Declaration
    l2cap_input( { 0x0E, 0x01, 0x00, 0x02, 0x00, 0x04, 0x00 } );
    expected_result( {
        0x0F,                                           // opcode
        0xA9, 0x3C, 0xC7, 0x5B, 0xED, 0x4E, 0x8A, 0xA2, // Primary Service
        0x9F, 0x49, 0xE2, 0x0D, 0x94, 0x40, 0x8B, 0x8C,
        0x0A, 0x03, 0x00,                               // first Characteristic Declaration; already clipped
        0xAA, 0x3C, 0xC7
                                                        // second Characteristic...
    } );
}

BOOST_AUTO_TEST_SUITE_END()
