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

class pairing_io_t
{
public:
    void init()
    {
        displayed_pass_key_ = ~0;
        response_ = nullptr;
    }

    pairing_io_t()
    {
        init();
    }

    void sm_pairing_yes_no( bluetoe::pairing_yes_no_response& response )
    {
        response_ = &response;
    }

    void sm_pairing_numeric_output( int pass_key )
    {
        displayed_pass_key_ = pass_key;
    }

    void response( bool r )
    {
        response_->yes_no_response( r );
        response_ = nullptr;
    }

private:
    int displayed_pass_key_;
    bluetoe::pairing_yes_no_response* response_;
} pairing_io;

template < class Manager >
struct with_display_and_yes_no_input : test::security_manager_base< Manager, test::all_security_functions, 65,
    bluetoe::pairing_numeric_output< pairing_io_t, pairing_io >,
    bluetoe::pairing_yes_no< pairing_io_t, pairing_io > >
{
    with_display_and_yes_no_input()
    {
        pairing_io.init();

        // Feature Exchange
        this->expected(
            {
                0x01,           // Pairing Request
                0x01,           // IO Capability DisplayYesNo
                0x00,           // OOB data flag (data not present)
                0x08,           // Bonding, MITM = 0, SC = 1, Keypress = 0
                0x10,           // Maximum Encryption Key Size (16)
                0x07,           // Initiator Key Distribution
                0x07,           // Responder Key Distribution (RFU)
            },
            {
                0x02,           // response
                0x01,           // DisplayYesNo
                0x00,           // OOB Authentication data not present
                0x08,           // Bonding, MITM = 0, SC = 1, Keypress = 0
                0x10,           // Maximum Encryption Key Size
                0x00,           // LinkKey
                0x00            // LinkKey
            }
        );

        // Public Key Exchange
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

        // LESC Pairing Confirm
        this->expected(
            {
                0x03,           // Pairing Confirm
                0x99, 0x57, 0x89, 0x9b,
                0xaa, 0x41, 0x1b, 0x45,
                0x15, 0x8d, 0x58, 0xff,
                0x73, 0x9b, 0x81, 0xd1
            }
        );

        // Pairing Random Exchange
        this->expected(
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
};

BOOST_AUTO_TEST_CASE_TEMPLATE( async_numeric_compairson_failed, Manager, test::lesc_managers )
{
    with_display_and_yes_no_input< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
            0x68, 0xd6, 0x70, 0x63,
            0xae, 0x09, 0x87, 0x88,
            0xa0, 0x19, 0x56, 0xa0,
            0xca, 0xf0, 0x5d, 0x9d
        },
        // No Output expected
        {}
    );

    pairing_io.response( false );

    fixture.expected(
        {
            0x05,           // Pairing Failed
            0x01            // Passkey Entry Failed
        }
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( async_numeric_compairson_success, Manager, test::lesc_managers )
{
    with_display_and_yes_no_input< Manager > fixture;

    fixture.expected(
        {
            0x0D,           // DHKey Check
            0x68, 0xd6, 0x70, 0x63,
            0xae, 0x09, 0x87, 0x88,
            0xa0, 0x19, 0x56, 0xa0,
            0xca, 0xf0, 0x5d, 0x9d
        },
        // No Output expected
        {}
    );

    pairing_io.response( true );

    fixture.expected(
        {
            0x0D,           // DHKey Check
            0xce, 0x16, 0xae, 0x4e,
            0x27, 0x54, 0x6b, 0x0d,
            0x07, 0x7d, 0xd1, 0xd6,
            0x07, 0xeb, 0x2d, 0x84
        }
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( sync_numeric_compairson_success, Manager, test::lesc_managers )
{
    with_display_and_yes_no_input< Manager > fixture;
    pairing_io.response( true );

    fixture.expected_ignoring_output_available(
        {
            0x0D,           // DHKey Check
            0x68, 0xd6, 0x70, 0x63,
            0xae, 0x09, 0x87, 0x88,
            0xa0, 0x19, 0x56, 0xa0,
            0xca, 0xf0, 0x5d, 0x9d
        },
        {
            0x0D,           // DHKey Check
            0xce, 0x16, 0xae, 0x4e,
            0x27, 0x54, 0x6b, 0x0d,
            0x07, 0x7d, 0xd1, 0xd6,
            0x07, 0xeb, 0x2d, 0x84
        }
    );

    std::uint8_t buffer[ 200 ];
    std::size_t  size = 200;
    fixture.l2cap_output( buffer, size , fixture.connection_data_ );
    BOOST_CHECK( size == 0 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( sync_numeric_compairson_fail, Manager, test::lesc_managers )
{
    with_display_and_yes_no_input< Manager > fixture;
    pairing_io.response( false );

    fixture.expected_ignoring_output_available(
        {
            0x0D,           // DHKey Check
            0x68, 0xd6, 0x70, 0x63,
            0xae, 0x09, 0x87, 0x88,
            0xa0, 0x19, 0x56, 0xa0,
            0xca, 0xf0, 0x5d, 0x9d
        },
        {
            0x05,           // Pairing Failed
            0x01            // Passkey Entry Failed
        }
    );

    std::uint8_t buffer[ 200 ];
    std::size_t  size = 200;
    fixture.l2cap_output( buffer, size , fixture.connection_data_ );
    BOOST_CHECK( size == 0 );
}
