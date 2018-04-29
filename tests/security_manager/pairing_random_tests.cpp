#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/sm/security_manager.hpp>

#include "test_sm.hpp"

BOOST_FIXTURE_TEST_CASE( wrong_state, test::pairing_features_exchanged )
{
    expected(
        {
            0x04,                   // opcode
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03
        },
        {
            0x05,                   // Pairing Failed
            0x08                    // Unspecified Reason
        }
    );
}

BOOST_FIXTURE_TEST_CASE( wrong_size, test::pairing_confirm_exchanged )
{
    expected(
        {
            0x04,                   // opcode
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02
        },
        {
            0x05,           // Pairing Failed
            0x0A            // Invalid Parameters
        }
    );
}

BOOST_FIXTURE_TEST_CASE( wrong_size_II, test::pairing_confirm_exchanged )
{
    expected(
        {
            0x04,                   // opcode
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0x00, 0x01, 0x02, 0x03,
            0xff
        },
        {
            0x05,           // Pairing Failed
            0x0A            // Invalid Parameters
        }
    );
}

BOOST_FIXTURE_TEST_CASE( incorrect_confirm_value, test::pairing_confirm_exchanged )
{
    expected(
        {
            0x04,                   // opcode
            0xE0, 0x2E, 0x70, 0xC6,
            0x4E, 0x27, 0x88, 0x63,
            0x0E, 0x6F, 0xAD, 0x56,
            0x21, 0xD5, 0x83, 0x56
        },
        {
            0x05,           // Pairing Failed
            0x04            // Confirm Value Failed
        }
    );
}

BOOST_FIXTURE_TEST_CASE( correct_confirm_value, test::pairing_confirm_exchanged )
{
    expected(
        {
            0x04,                   // opcode
            0xE0, 0x2E, 0x70, 0xC6,
            0x4E, 0x27, 0x88, 0x63,
            0x0E, 0x6F, 0xAD, 0x56,
            0x21, 0xD5, 0x83, 0x57
        },
        {
            0x04,                   // opcode
            0xE0, 0x2E, 0x70, 0xC6,
            0x4E, 0x27, 0x88, 0x63,
            0x0E, 0x6F, 0xAD, 0x56,
            0x21, 0xD5, 0x83, 0x57
        }
    );
}
