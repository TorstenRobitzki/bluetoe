#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_dhkey_check_too_small, Manager, test::lesc_managers )
{
    test::lesc_pairing_random_exchanged< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
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

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_dhkey_check_too_large, Manager, test::lesc_managers )
{
    test::lesc_pairing_random_exchanged< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
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

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_dhkey_check_wrong_state, Manager, test::lesc_managers )
{
    test::lesc_pairing_confirmed< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        },
        {
            0x05,           // Pairing Failed
            0x08,           // Unspecified Reason
        }
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_dhkey_check_failed, Manager, test::lesc_managers )
{
    test::lesc_pairing_random_exchanged< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
            0x8a, 0x66, 0x11, 0x68,
            0x49, 0xca, 0x39, 0x2d,
            0x5d, 0x9b, 0xe1, 0x1e,
            0x42, 0x38, 0xef, 0xd8  // <- last byte is of by 1
        },
        {
            0x05,           // Pairing Failed
            0x0b,           // DHKey check failed
        }
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_dhkey_check_success, Manager, test::lesc_managers )
{
    test::lesc_pairing_random_exchanged< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
            0x68, 0xd6, 0x70, 0x63,
            0xae, 0x09, 0x87, 0x88,
            0xa0, 0x19, 0x56, 0xa0,
            0xca, 0xf0, 0x5d, 0x9d
        },
        {
            0x0D,           // DHKey Check
            0xd4, 0x5b, 0xa2, 0x51,
            0x11, 0x44, 0xd4, 0x69,
            0x30, 0x2a, 0xe6, 0x43,
            0x1d, 0x9a, 0x44, 0x1a
        }
    );

    BOOST_CHECK( fixture.connection_data().state() == bluetoe::details::sm_pairing_state::pairing_completed );
}
