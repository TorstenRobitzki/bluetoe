#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

BOOST_FIXTURE_TEST_CASE( invalid_state, test::legacy_security_manager<> )
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

BOOST_FIXTURE_TEST_CASE( invalid_size, test::legacy_pairing_features_exchanged )
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

BOOST_FIXTURE_TEST_CASE( invalid_size_II, test::legacy_pairing_features_exchanged )
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

/**
 * The original test vector is:
 *
 * 8-bit iat’ is 0x01
 * 8-bit rat’ is 0x00
 * 56-bit preq is 0x07071000000101
 * 56 bit pres is 0x05000800000302
 * p1 is 0x05000800000302070710000001010001
 * 48-bit ia is 0xA1A2A3A4A5A6
 * 48-bit ra is 0xB1B2B3B4B5B6
 * p2 is 0x00000000A1A2A3A4A5A6B1B2B3B4B5B6
 * 128-bit k is 0x00000000000000000000000000000000
 * 128-bit value r is 0x5783D52156AD6F0E6388274EC6702EE0
 * 128-bit output from the c1 function is 0x1e1e3fef878988ead2a74dc5bef13b86
 *
 * changed:
 * 56 bit pres is 0x00001000000302
 */
BOOST_FIXTURE_TEST_CASE( correct_paring_request, test::legacy_security_manager<> )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x00, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x00, 0x10, 0x00, 0x00
        }
    );

    expected(
        {
            0x03,                   // Pairing Confirm
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        },
        {
            0x03,                   // Pairing Confirm
            0xe2, 0x16, 0xb5, 0x44, // Confirm Value
            0x96, 0xa2, 0xe7, 0x90,
            0x53, 0xb2, 0x31, 0x06,
            0xc9, 0xdd, 0xd4, 0xf8
        }
    );
}
