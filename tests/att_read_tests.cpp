#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"


/**
 * @brief given that the Primary Service Declaration is the first ATT record, we can read it with a Read by Group Type Request
 */
BOOST_FIXTURE_TEST_CASE( primary_service_declaration_read_by_group_type, small_temperature_service_with_response<> )
{
    static const std::uint8_t request[] = { 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 };

    l2cap_input( request );
}