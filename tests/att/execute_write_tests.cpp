#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( execute_write_errors )

std::uint8_t value = 0;

typedef bluetoe::server<
    bluetoe::shared_write_queue< 100 >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( value ), &value >
        >
    >
> value_server;

BOOST_FIXTURE_TEST_CASE( pdu_to_small, request_with_reponse< value_server > )
{
    BOOST_CHECK( check_error_response( { 0x18 }, 0x18, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( pdu_to_large, request_with_reponse< value_server > )
{
    BOOST_CHECK( check_error_response( { 0x18, 0x00, 0x01 }, 0x18, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( invalid_flags, request_with_reponse< value_server > )
{
    BOOST_CHECK( check_error_response( { 0x18, 0x02 }, 0x18, 0x0000, 0x04 ) );
    BOOST_CHECK( check_error_response( { 0x18, 0x80 }, 0x18, 0x0000, 0x04 ) );
}

BOOST_FIXTURE_TEST_CASE( request_not_supported_if_no_buffer_exist, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x18, 0x00 }, 0x18, 0x0000, 0x06 ) );
}

BOOST_AUTO_TEST_SUITE_END()
