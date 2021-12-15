#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

BOOST_FIXTURE_TEST_CASE( pairing_confirm, test::lesc_public_key_exchanged )
{
    expected(
        {
            0x03,           // Pairing Confirm
            0x99, 0x57, 0x89, 0x9b,
            0xaa, 0x41, 0x1b, 0x45,
            0x15, 0x8d, 0x58, 0xff,
            0x73, 0x9b, 0x81, 0xd1
        }
    );
}

BOOST_FIXTURE_TEST_CASE( pairing_random_too_small, test::lesc_pairing_confirmed )
{
    expected(
        {
            0x04,           // Pairing Random
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00
        },
        {
            0x05,           // Pairing Failed
            0x0A,           // Invalid Parameters
        }
    );
}

BOOST_FIXTURE_TEST_CASE( pairing_random_too_large, test::lesc_pairing_confirmed )
{
    expected(
        {
            0x04,           // Pairing Random
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00
        },
        {
            0x05,           // Pairing Failed
            0x0A,           // Invalid Parameters
        }
    );
}

BOOST_FIXTURE_TEST_CASE( pairing_random, test::lesc_pairing_confirmed )
{
    // Na, Nb
    expected(
        // initiator nonce is not part of the Pairing confirm value)
        {
            0x04,           // Pairing Random
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        },
        {
            0x04,           // Pairing Random
            0xab, 0xae, 0x2b, 0x71,
            0xec, 0xb2, 0xff, 0xff,
            0x3e, 0x73, 0x77, 0xd1,
            0x54, 0x84, 0xcb, 0xd5
        }
    );
}