#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( read_multiple_errors )

BOOST_FIXTURE_TEST_CASE( pdu_to_small, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00 }, 0x0E, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_half_an_handle, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x03, 0x00, 0x04 }, 0x0E, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( the_first_handle_is_invalid, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00 }, 0x0E, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( the_second_handle_is_invalid, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00 }, 0x0E, 0x0000, 0x01 ) );
}

BOOST_FIXTURE_TEST_CASE( the_first_handle_is_unknown, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x80, 0x03, 0x00, 0x04, 0x00 }, 0x0E, 0x8002, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( the_second_handle_is_unknown, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x03, 0x00, 0xf4, 0xff }, 0x0E, 0xfff4, 0x0A ) );
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
            bluetoe::no_read_access
        >
    >
> unreadable_server;


BOOST_FIXTURE_TEST_CASE( first_attribute_not_readable, request_with_reponse< unreadable_server > )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x03, 0x00, 0x02, 0x00 }, 0x0E, 0x0003, 0x02 ) );
}

BOOST_FIXTURE_TEST_CASE( last_attribute_not_readable, request_with_reponse< unreadable_server > )
{
    BOOST_CHECK( check_error_response( { 0x0E, 0x02, 0x00, 0x03, 0x00 }, 0x0E, 0x0003, 0x02 ) );
}

BOOST_AUTO_TEST_SUITE_END()
