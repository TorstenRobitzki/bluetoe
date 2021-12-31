#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"


BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_confirm, Manager, test::lesc_managers )
{
    test::lesc_public_key_exchanged< Manager > fixture;

    fixture.expected(
        {
            0x03,           // Pairing Confirm
            0x99, 0x57, 0x89, 0x9b,
            0xaa, 0x41, 0x1b, 0x45,
            0x15, 0x8d, 0x58, 0xff,
            0x73, 0x9b, 0x81, 0xd1
        }
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_random_too_small, Manager, test::lesc_managers )
{
    test::lesc_pairing_confirmed< Manager > fixture;

    fixture.expected(
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

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_random_too_large, Manager, test::lesc_managers )
{
    test::lesc_pairing_confirmed< Manager > fixture;

    fixture.expected(
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

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_random_wrong_state, Manager, test::lesc_managers )
{
    test::lesc_public_key_exchanged< Manager > fixture;

    fixture.expected_ignoring_output_available(
        {
            0x04,           // Pairing Random
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
        },
        {
            0x05,           // Pairing Failed
            0x08,           // Unspecified Reason
        }
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( pairing_random, Manager, test::lesc_managers )
{
    test::lesc_pairing_confirmed< Manager > fixture;

    // Na, Nb
    fixture.expected(
        // initiator nonce is not part of the Pairing confirm value)
        {
            0x04,           // Pairing Random
            0xcf, 0xc4, 0x3d, 0xff,
            0xf7, 0x83, 0x65, 0x21,
            0x6e, 0x5f, 0xa7, 0x25,
            0xcc, 0xe7, 0xe8, 0xa6
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

struct pass_key_display_t {
    pass_key_display_t()
        : displayed_pass_key( ~0 )
        , called( false )
    {
    }

    void sm_pairing_numeric_output( int pass_key )
    {
        BOOST_REQUIRE( !called );
        called = true;
        displayed_pass_key = pass_key;
    }

    int displayed_pass_key;
    bool called;
} pass_key_display;

struct yes_no_input_t
{
    bool sm_pairing_yes_no()
    {
        return yes_no_response;
    }

    bool yes_no_response = true;
} yes_no_input;

using lesc_security_manager_with_display_and_yes_no = test::lesc_security_manager< 65,
    bluetoe::pairing_numeric_output< pass_key_display_t, pass_key_display >,
    bluetoe::pairing_yes_no< yes_no_input_t, yes_no_input > >;

using security_manager_with_display_and_yes_no = test::security_manager< 65,
    bluetoe::pairing_numeric_output< pass_key_display_t, pass_key_display >,
    bluetoe::pairing_yes_no< yes_no_input_t, yes_no_input > >;


using lesc_security_managers_with_display_and_yes_no = std::tuple<
    lesc_security_manager_with_display_and_yes_no,
    security_manager_with_display_and_yes_no
>;

template < bool YesNoResponse, class BaseFixture >
struct lesc_security_manager_with_display_and_yes_no_paired : BaseFixture
{
    lesc_security_manager_with_display_and_yes_no_paired()
    {
        pass_key_display.called = false;
        yes_no_input.yes_no_response = YesNoResponse;

        this->expected(
            {
                0x01,       // Pairing request
                0x01,       // DisplayYesNo
                0x00,       // OOB Authentication data not present
                0x08,       // No Bonding, No MITM, SC = 1
                0x10,       // Maximum Encryption Key Size
                0x00,       // Initiator Key Distribution
                0x00        // Responder Key Distribution
            },
            {
                0x02,       // Pairing Response
                0x01,       // DisplayYesNo
                0x00,       // OOB Authentication data not present
                0x08,       // No Bonding, MITM = 0, SC = 1, Keypress = 0
                0x10,       // Maximum Encryption Key Size
                0x00,       // Initiator Key Distribution
                0x00        // Responder Key Distribution
            }
        );
    }
};

BOOST_AUTO_TEST_CASE_TEMPLATE( lesc_numeric_comparison_pairing_public_key_exchange, Fixture, lesc_security_managers_with_display_and_yes_no )
{
    lesc_security_manager_with_display_and_yes_no_paired< true, Fixture > fixture;

    fixture.expected(
        {
            0x0C,                   // Pairing Public Key
            // Public Key X
            0xe6, 0x9d, 0x35, 0x0e,
            0x48, 0x01, 0x03, 0xcc,
            0xdb, 0xfd, 0xf4, 0xac,
            0x11, 0x91, 0xf4, 0xef,
            0xb9, 0xa5, 0xf9, 0xe9,
            0xa7, 0x83, 0x2c, 0x5e,
            0x2c, 0xbe, 0x97, 0xf2,
            0xd2, 0x03, 0xb0, 0x20,
            // Public Key Y
            0x8b, 0xd2, 0x89, 0x15,
            0xd0, 0x8e, 0x1c, 0x74,
            0x24, 0x30, 0xed, 0x8f,
            0xc2, 0x45, 0x63, 0x76,
            0x5c, 0x15, 0x52, 0x5a,
            0xbf, 0x9a, 0x32, 0x63,
            0x6d, 0xeb, 0x2a, 0x65,
            0x49, 0x9c, 0x80, 0xdc
        },
        {
            0x0C,                   // Pairing Public Key
            // Public Key X
            0x90, 0xa1, 0xaa, 0x2f,
            0xb2, 0x77, 0x90, 0x55,
            0x9f, 0xa6, 0x15, 0x86,
            0xfd, 0x8a, 0xb5, 0x47,
            0x00, 0x4c, 0x9e, 0xf1,
            0x84, 0x22, 0x59, 0x09,
            0x96, 0x1d, 0xaf, 0x1f,
            0xf0, 0xf0, 0xa1, 0x1e,
            // Public Key Y
            0x4a, 0x21, 0xb1, 0x15,
            0xf9, 0xaf, 0x89, 0x5f,
            0x76, 0x36, 0x8e, 0xe2,
            0x30, 0x11, 0x2d, 0x47,
            0x60, 0x51, 0xb8, 0x9a,
            0x3a, 0x70, 0x56, 0x73,
            0x37, 0xad, 0x9d, 0x42,
            0x3e, 0xf3, 0x55, 0x4c,
        }
    );
}

template < bool YesNoResponse, class BaseFixture >
struct lesc_security_manager_with_display_and_yes_no_key_exchanged : lesc_security_manager_with_display_and_yes_no_paired< YesNoResponse, BaseFixture >
{
    lesc_security_manager_with_display_and_yes_no_key_exchanged()
    {
        this->expected(
            {
                0x0C,                   // Pairing Public Key
                // Public Key X
                0xe6, 0x9d, 0x35, 0x0e,
                0x48, 0x01, 0x03, 0xcc,
                0xdb, 0xfd, 0xf4, 0xac,
                0x11, 0x91, 0xf4, 0xef,
                0xb9, 0xa5, 0xf9, 0xe9,
                0xa7, 0x83, 0x2c, 0x5e,
                0x2c, 0xbe, 0x97, 0xf2,
                0xd2, 0x03, 0xb0, 0x20,
                // Public Key Y
                0x8b, 0xd2, 0x89, 0x15,
                0xd0, 0x8e, 0x1c, 0x74,
                0x24, 0x30, 0xed, 0x8f,
                0xc2, 0x45, 0x63, 0x76,
                0x5c, 0x15, 0x52, 0x5a,
                0xbf, 0x9a, 0x32, 0x63,
                0x6d, 0xeb, 0x2a, 0x65,
                0x49, 0x9c, 0x80, 0xdc
            },
            {
                0x0C,                   // Pairing Public Key
                // Public Key X
                0x90, 0xa1, 0xaa, 0x2f,
                0xb2, 0x77, 0x90, 0x55,
                0x9f, 0xa6, 0x15, 0x86,
                0xfd, 0x8a, 0xb5, 0x47,
                0x00, 0x4c, 0x9e, 0xf1,
                0x84, 0x22, 0x59, 0x09,
                0x96, 0x1d, 0xaf, 0x1f,
                0xf0, 0xf0, 0xa1, 0x1e,
                // Public Key Y
                0x4a, 0x21, 0xb1, 0x15,
                0xf9, 0xaf, 0x89, 0x5f,
                0x76, 0x36, 0x8e, 0xe2,
                0x30, 0x11, 0x2d, 0x47,
                0x60, 0x51, 0xb8, 0x9a,
                0x3a, 0x70, 0x56, 0x73,
                0x37, 0xad, 0x9d, 0x42,
                0x3e, 0xf3, 0x55, 0x4c,
            }
        );
    }
};

BOOST_AUTO_TEST_CASE_TEMPLATE( lesc_numeric_comparison_commitment_and_random, Fixture, lesc_security_managers_with_display_and_yes_no )
{
    lesc_security_manager_with_display_and_yes_no_key_exchanged< true, Fixture > fixture;

    fixture.expected(
        {
            0x03,           // Pairing Confirm
            0x99, 0x57, 0x89, 0x9b,
            0xaa, 0x41, 0x1b, 0x45,
            0x15, 0x8d, 0x58, 0xff,
            0x73, 0x9b, 0x81, 0xd1
        }
    );

    // Na, Nb
    fixture.expected(
        // initiator nonce is not part of the Pairing confirm value)
        {
            0x04,           // Pairing Random
            0xcf, 0xc4, 0x3d, 0xff,
            0xf7, 0x83, 0x65, 0x21,
            0x6e, 0x5f, 0xa7, 0x25,
            0xcc, 0xe7, 0xe8, 0xa6
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

template < bool YesNoResponse, class BaseFixture >
struct lesc_security_manager_with_display_and_yes_commitment_and_random : lesc_security_manager_with_display_and_yes_no_key_exchanged< YesNoResponse, BaseFixture >
{
    lesc_security_manager_with_display_and_yes_commitment_and_random()
    {
        this->expected(
            {
                0x03,           // Pairing Confirm
                0x99, 0x57, 0x89, 0x9b,
                0xaa, 0x41, 0x1b, 0x45,
                0x15, 0x8d, 0x58, 0xff,
                0x73, 0x9b, 0x81, 0xd1
            }
        );

        // Na, Nb
        this->expected(
            // initiator nonce is not part of the Pairing confirm value)
            {
                0x04,           // Pairing Random
                0xcf, 0xc4, 0x3d, 0xff,
                0xf7, 0x83, 0x65, 0x21,
                0x6e, 0x5f, 0xa7, 0x25,
                0xcc, 0xe7, 0xe8, 0xa6
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
};

BOOST_AUTO_TEST_CASE_TEMPLATE( lesc_numeric_comparison_user_check_ok, Fixture, lesc_security_managers_with_display_and_yes_no )
{
    lesc_security_manager_with_display_and_yes_commitment_and_random< true, Fixture > fixture;

    const std::array< std::uint8_t, 32 > pka = {{
        0xe6, 0x9d, 0x35, 0x0e,
        0x48, 0x01, 0x03, 0xcc,
        0xdb, 0xfd, 0xf4, 0xac,
        0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9,
        0xa7, 0x83, 0x2c, 0x5e,
        0x2c, 0xbe, 0x97, 0xf2,
        0xd2, 0x03, 0xb0, 0x20
    }};

    const std::array< std::uint8_t, 32 > pkb = {{
        0x90, 0xa1, 0xaa, 0x2f,
        0xb2, 0x77, 0x90, 0x55,
        0x9f, 0xa6, 0x15, 0x86,
        0xfd, 0x8a, 0xb5, 0x47,
        0x00, 0x4c, 0x9e, 0xf1,
        0x84, 0x22, 0x59, 0x09,
        0x96, 0x1d, 0xaf, 0x1f,
        0xf0, 0xf0, 0xa1, 0x1e
    }};

    const std::array< std::uint8_t, 16 > na = {{
        0xcf, 0xc4, 0x3d, 0xff,
        0xf7, 0x83, 0x65, 0x21,
        0x6e, 0x5f, 0xa7, 0x25,
        0xcc, 0xe7, 0xe8, 0xa6
    }};

    const std::array< std::uint8_t, 16 > nb = {{
        0xab, 0xae, 0x2b, 0x71,
        0xec, 0xb2, 0xff, 0xff,
        0x3e, 0x73, 0x77, 0xd1,
        0x54, 0x84, 0xcb, 0xd5
    }};

    const auto bit20mask = 0xfffff;

    const auto expected_key = fixture.g2( pka.data(), pkb.data(), na, nb );
    BOOST_CHECK( pass_key_display.called );
    BOOST_CHECK_EQUAL(
        static_cast< int >( pass_key_display.displayed_pass_key & bit20mask ),
        static_cast< int >( expected_key & bit20mask ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( lesc_numeric_comparison_user_check_fail, Fixture, lesc_security_managers_with_display_and_yes_no )
{
    lesc_security_manager_with_display_and_yes_no_key_exchanged< false, Fixture > fixture;

    fixture.expected(
        {
            0x03,           // Pairing Confirm
            0x99, 0x57, 0x89, 0x9b,
            0xaa, 0x41, 0x1b, 0x45,
            0x15, 0x8d, 0x58, 0xff,
            0x73, 0x9b, 0x81, 0xd1
        }
    );

    // Na, Nb
    fixture.expected(
        // initiator nonce is not part of the Pairing confirm value)
        {
            0x04,           // Pairing Random
            0xcf, 0xc4, 0x3d, 0xff,
            0xf7, 0x83, 0x65, 0x21,
            0x6e, 0x5f, 0xa7, 0x25,
            0xcc, 0xe7, 0xe8, 0xa6
        },
        {
            0x05,           // Pairing failed
            0x01            // Passkey Entry Failed
        }
    );
}
