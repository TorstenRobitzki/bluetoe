#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

BOOST_FIXTURE_TEST_CASE( make_sure_unsupported_requests_are_answered_correct, small_temperature_service_with_response<> )
{
    BOOST_CHECK( check_error_response( { 0x65 }, 0x65, 0x0000, 0x06 ) );
}
