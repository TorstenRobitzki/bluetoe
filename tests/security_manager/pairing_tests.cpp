#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/sm/security_manager.hpp>

#include "../test_servers.hpp"

template < class Manager, std::size_t MTU = 27 >
struct link_layer : Manager
{
    void expected(
        std::initializer_list< std::uint8_t > input,
        std::initializer_list< std::uint8_t > expected_output )
    {
        std::uint8_t buffer[ MTU ];
        std::size_t  size = MTU;

        this->l2cap_input( input.begin(), input.size(), &buffer[ 0 ], size );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            expected_output.begin(), expected_output.end(),
            &buffer[ 0 ], &buffer[ size ] );
    }
};

BOOST_FIXTURE_TEST_CASE( no_response_from_the_no_security_manager_implementation, link_layer< bluetoe::no_security_manager > )
{
    expected(
        { 0x01, 0x03 },
        {}
    );
}