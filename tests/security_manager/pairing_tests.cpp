#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

//#include <server.hpp>
#include <security_manager.hpp>
#include <test_servers.hpp>

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

static const std::initializer_list< std::uint8_t > pairing_request = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

BOOST_FIXTURE_TEST_CASE( no_security_manager_no_pairing, link_layer< bluetoe::no_security_manager > )
{
    expected(
        pairing_request,
        {
            0x05, 0x05
        }
    );
}

BOOST_FIXTURE_TEST_CASE( by_default_no_oob_no_lesc, link_layer< bluetoe::security_manager > )
{
    expected(
        pairing_request,
        {
            0x02,   // response
            0x03,   // NoInputNoOutput
            0x00,   // OOB Authentication data not present
            0x40,   // Bonding, MITM = 0, SC = 0, Keypress = 0
            0x10,   // Maximum Encryption Key Size
            0x0f,   // LinkKey
            0x0f    // LinkKey
        }
    );
}
