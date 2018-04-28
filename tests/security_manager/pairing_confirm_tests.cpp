#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

template < class Manager, std::size_t MTU = 27 >
using sm = test::security_manager< Manager, MTU >;

BOOST_FIXTURE_TEST_CASE( invalid_state, sm< bluetoe::security_manager > )
{
    expected(
        {
            0x03,                   // Pairing Confirm
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
        },
        {
            0x05,                   // Pairing Failed
            0x08                    // Unspecified Reason
        }
    );
}

BOOST_FIXTURE_TEST_CASE( invalid_size, test::pairing_features_exchanged )
{
    expected(
        {
            0x03,                   // Pairing Confirm
        },
        {
            0x05,                   // Pairing Failed
            0x0A                    // Invalid Parameters
        }
    );
}

BOOST_FIXTURE_TEST_CASE( invalid_size_II, test::pairing_features_exchanged )
{
    expected(
        {
            0x03,                   // Pairing Confirm
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00
        },
        {
            0x05,                   // Pairing Failed
            0x0A                    // Invalid Parameters
        }
    );
}
