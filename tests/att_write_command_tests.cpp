#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( write_errors )

BOOST_FIXTURE_TEST_CASE( pdu_to_small, small_temperature_service_with_response<> )
{
    l2cap_input( { 0x52, 0x02 } );
    expected_result( {} );
}

BOOST_FIXTURE_TEST_CASE( no_such_handle, small_temperature_service_with_response<> )
{
    l2cap_input( { 0x52, 0x17, 0xAA, 0x01 } );
    expected_result( {} );
}

BOOST_FIXTURE_TEST_CASE( invalid_handle, small_temperature_service_with_response<> )
{
    l2cap_input( { 0x52, 0x00, 0x00 } );
    expected_result( {} );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( write_command )

std::uint32_t value = 0x0000;

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( value ), &value >
        >
    >
> small_value_server;

BOOST_FIXTURE_TEST_CASE( write_full_data, request_with_reponse< small_value_server > )
{
    value = 0x3512;
    l2cap_input( { 0x52, 0x03, 0x00, 0x01, 0x02, 0x03, 0x04 } );
    expected_result( {} );
    BOOST_CHECK_EQUAL( value, 0x04030201 );
}

BOOST_FIXTURE_TEST_CASE( write_full_data_part, request_with_reponse< small_value_server > )
{
    value = 0x44332211;
    l2cap_input( { 0x52, 0x03, 0x00, 0x01, 0x02, 0x03 } );
    expected_result( {} );
    BOOST_CHECK_EQUAL( value, 0x44030201 );
}

BOOST_AUTO_TEST_SUITE_END()
