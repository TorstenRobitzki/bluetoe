#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

BOOST_FIXTURE_TEST_SUITE( pairing_public_key_exchange, test::lesc_pairing_features_exchanged )

    BOOST_AUTO_TEST_CASE( request_too_small )
    {
        expected(
            {
                0x0C,           // Pairing Public Key
                // Public Key X
                0x20, 0xb0, 0x03, 0xd2,
                0xf2, 0x97, 0xbe, 0x2c,
                0x5e, 0x2c, 0x83, 0xa7,
                0xe9, 0xf9, 0xa5, 0xb9,
                0xef, 0xf4, 0x91, 0x11,
                0xac, 0xf4, 0xfd, 0xdb,
                0xcc, 0x03, 0x01, 0x48,
                0x0e, 0x35, 0x9d, 0xe6,
                // Public Key Y
                0xdc, 0x80, 0x9c, 0x49,
                0x65, 0x2a, 0xeb, 0x6d,
                0x63, 0x32, 0x9a, 0xbf,
                0x5a, 0x52, 0x15, 0x5c,
                0x76, 0x63, 0x45, 0xc2,
                0x8f, 0xed, 0x30, 0x24,
                0x74, 0x1c, 0x8e, 0xd0,
                0x15, 0x89, 0xd2
            },
            {
                0x05,           // Pairing Failed
                0x0A,           // Invalid Parameters
            }
        );
    }


    BOOST_AUTO_TEST_CASE( request_too_large )
    {
        expected(
            {
                0x0C,           // Pairing Public Key
                // Public Key X
                0x20, 0xb0, 0x03, 0xd2,
                0xf2, 0x97, 0xbe, 0x2c,
                0x5e, 0x2c, 0x83, 0xa7,
                0xe9, 0xf9, 0xa5, 0xb9,
                0xef, 0xf4, 0x91, 0x11,
                0xac, 0xf4, 0xfd, 0xdb,
                0xcc, 0x03, 0x01, 0x48,
                0x0e, 0x35, 0x9d, 0xe6,
                // Public Key Y
                0xdc, 0x80, 0x9c, 0x49,
                0x65, 0x2a, 0xeb, 0x6d,
                0x63, 0x32, 0x9a, 0xbf,
                0x5a, 0x52, 0x15, 0x5c,
                0x76, 0x63, 0x45, 0xc2,
                0x8f, 0xed, 0x30, 0x24,
                0x74, 0x1c, 0x8e, 0xd0,
                0x15, 0x89, 0xd2, 0x8b,
                0x00                    // extra byte
            },
            {
                0x05,           // Pairing Failed
                0x0A,           // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE( key_not_on_valid_curve )
    {
        expected(
            {
                0x0C,           // Pairing Public Key
                // Public Key X
                0x20, 0xb0, 0x03, 0xd2,
                0xf2, 0x97, 0xbe, 0x2c,
                0x5e, 0x2c, 0x83, 0xa7,
                0xe9, 0xf9, 0xa5, 0xb9,
                0xef, 0xf4, 0x91, 0x11,
                0xac, 0xf4, 0xfd, 0xdb,
                0xcc, 0x03, 0x01, 0x48,
                0x0e, 0x35, 0x9d, 0xe6,
                // Public Key Y
                0xdc, 0x80, 0x9c, 0x49,
                0x65, 0x2a, 0xeb, 0x6d,
                0x63, 0x32, 0x9a, 0xbf,
                0x5a, 0x52, 0x15, 0x5c,
                0x76, 0x63, 0x45, 0xc2,
                0x8f, 0xed, 0x30, 0x24,
                0x74, 0x1c, 0x8e, 0xd0,
                0x15, 0x89, 0xd2, 0x80  // <-- last nibble changed
            },
            {
                0x05,           // Pairing Failed
                0x0A,           // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE( valid_key )
    {
        expected(
            {
                0x0C,                   // Pairing Public Key
                // Public Key X
                0x20, 0xb0, 0x03, 0xd2,
                0xf2, 0x97, 0xbe, 0x2c,
                0x5e, 0x2c, 0x83, 0xa7,
                0xe9, 0xf9, 0xa5, 0xb9,
                0xef, 0xf4, 0x91, 0x11,
                0xac, 0xf4, 0xfd, 0xdb,
                0xcc, 0x03, 0x01, 0x48,
                0x0e, 0x35, 0x9d, 0xe6,
                // Public Key Y
                0xdc, 0x80, 0x9c, 0x49,
                0x65, 0x2a, 0xeb, 0x6d,
                0x63, 0x32, 0x9a, 0xbf,
                0x5a, 0x52, 0x15, 0x5c,
                0x76, 0x63, 0x45, 0xc2,
                0x8f, 0xed, 0x30, 0x24,
                0x74, 0x1c, 0x8e, 0xd0,
                0x15, 0x89, 0xd2, 0x8b
            },
            {
                0x0C,                   // Pairing Public Key
                // Public Key X
                0x20, 0xb0, 0x03, 0xd2,
                0xf2, 0x97, 0xbe, 0x2c,
                0x5e, 0x2c, 0x83, 0xa7,
                0xe9, 0xf9, 0xa5, 0xb9,
                0xef, 0xf4, 0x91, 0x11,
                0xac, 0xf4, 0xfd, 0xdb,
                0xcc, 0x03, 0x01, 0x48,
                0x0e, 0x35, 0x9d, 0xe6,
                // Public Key Y
                0xdc, 0x80, 0x9c, 0x49,
                0x65, 0x2a, 0xeb, 0x6d,
                0x63, 0x32, 0x9a, 0xbf,
                0x5a, 0x52, 0x15, 0x5c,
                0x76, 0x63, 0x45, 0xc2,
                0x8f, 0xed, 0x30, 0x24,
                0x74, 0x1c, 0x8e, 0xd0,
                0x15, 0x89, 0xd2, 0x8b
            }
        );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_CASE( not_expected_key_exchanged, test::lesc_security_manager<> )
{
    expected(
        {
            0x0C,                   // Pairing Public Key
            // Public Key X
            0x20, 0xb0, 0x03, 0xd2,
            0xf2, 0x97, 0xbe, 0x2c,
            0x5e, 0x2c, 0x83, 0xa7,
            0xe9, 0xf9, 0xa5, 0xb9,
            0xef, 0xf4, 0x91, 0x11,
            0xac, 0xf4, 0xfd, 0xdb,
            0xcc, 0x03, 0x01, 0x48,
            0x0e, 0x35, 0x9d, 0xe6,
            // Public Key Y
            0xdc, 0x80, 0x9c, 0x49,
            0x65, 0x2a, 0xeb, 0x6d,
            0x63, 0x32, 0x9a, 0xbf,
            0x5a, 0x52, 0x15, 0x5c,
            0x76, 0x63, 0x45, 0xc2,
            0x8f, 0xed, 0x30, 0x24,
            0x74, 0x1c, 0x8e, 0xd0,
            0x15, 0x89, 0xd2, 0x8b
        },
        {
            0x05,           // Pairing Failed
            0x08,           // Unspecified Reason
        }
    );
}
