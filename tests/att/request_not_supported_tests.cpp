#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_FIXTURE_TEST_CASE( make_sure_unsupported_requests_are_answered_correct, test::small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x65 }, 0x65, 0x0000, 0x06 ) );
}

BOOST_FIXTURE_TEST_CASE( make_sure_error_response_is_not_responsed_with_an_error_response, test::small_temperature_service_with_response<> )
{
    l2cap_input( { 0x01, 0x01, 0x00, 0x00, 0x06 } );
    expected_result( {} );
}
