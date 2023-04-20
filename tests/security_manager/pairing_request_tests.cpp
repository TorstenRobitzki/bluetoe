#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

using security_managers_under_test = std::tuple<
    test::legacy_security_manager<>,
    test::lesc_security_manager<> >;

BOOST_AUTO_TEST_SUITE( invalid_pairing_requests )

    BOOST_AUTO_TEST_CASE_TEMPLATE( empty_request, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_opcode, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x55
            },
            {
                0x05,           // Pairing Failed
                0x07            // Command Not Supported
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( no_parameters, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01            // Pairing Request
            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_IO_Capability, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01,           // Pairing Request
                0x05,           // IO Capability RFU
                0x00,           // OOB data flag (data not present)
                0x00,           // AuthReq
                0x10,           // Maximum Encryption Key Size (16)
                0x00,           // Initiator Key Distribution
                0x00,           // Responder Key Distribution

            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_OOB_data_flag, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01,           // Pairing Request
                0x03,           // IO Capability NoInputNoOutput
                0x02,           // OOB data flag (RFU)
                0x00,           // AuthReq
                0x10,           // Maximum Encryption Key Size (16)
                0x00,           // Initiator Key Distribution
                0x00,           // Responder Key Distribution

            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_Encryption_Key_Size, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01,           // Pairing Request
                0x03,           // IO Capability NoInputNoOutput
                0x00,           // OOB data flag (data not present)
                0x00,           // AuthReq
                0x11,           // Maximum Encryption Key Size (17)
                0x00,           // Initiator Key Distribution
                0x00,           // Responder Key Distribution

            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_Encryption_Key_Size_II, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01,           // Pairing Request
                0x03,           // IO Capability NoInputNoOutput
                0x00,           // OOB data flag (data not present)
                0x00,           // AuthReq
                0x06,           // Maximum Encryption Key Size (6)
                0x00,           // Initiator Key Distribution
                0x00,           // Responder Key Distribution

            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_Initiator_Key_Distribution, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01,           // Pairing Request
                0x03,           // IO Capability NoInputNoOutput
                0x00,           // OOB data flag (data not present)
                0x00,           // AuthReq
                0x10,           // Maximum Encryption Key Size (16)
                0xF0,           // Initiator Key Distribution (RFU)
                0x00,           // Responder Key Distribution

            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE_TEMPLATE( invalid_Responder_Key_Distribution, Manager, security_managers_under_test )
    {
        Manager mng;
        mng.expected(
            {
                0x01,           // Pairing Request
                0x03,           // IO Capability NoInputNoOutput
                0x00,           // OOB data flag (data not present)
                0x00,           // AuthReq
                0x10,           // Maximum Encryption Key Size (16)
                0x00,           // Initiator Key Distribution
                0xF0,           // Responder Key Distribution (RFU)
            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE_TEMPLATE( not_in_idle_state, Manager, test::legacy_managers )
{
    test::legacy_pairing_features_exchanged< Manager > fixture;

    fixture.expected(
        {
            0x01,           // Pairing Request
            0x03,           // IO Capability NoInputNoOutput
            0x00,           // OOB data flag (data not present)
            0x00,           // AuthReq
            0x10,           // Maximum Encryption Key Size (16)
            0x00,           // Initiator Key Distribution
            0x00,           // Responder Key Distribution (RFU)
        },
        {
            0x05,           // Pairing Failed
            0x08            // Unspecified Reason
        }
    );
}

BOOST_FIXTURE_TEST_CASE( legacy_not_supported, test::lesc_security_manager<> )
{
    expected(
        {
            0x01,           // Pairing Request
            0x01,           // IO Capability NoInputNoOutput
            0x00,           // OOB data flag (data not present)
            0x00,           // AuthReq, SC not set!
            0x10,           // Maximum Encryption Key Size (16)
            0x07,           // Initiator Key Distribution
            0x07,           // Responder Key Distribution (RFU)
        },
        {
            0x05,           // Pairing Failed
            0x05            // Pairing Not Supported
        }
    );
}

BOOST_AUTO_TEST_SUITE( auth_req_field )

using legacy_bonding_response = test::legacy_security_manager< 23, bluetoe::enable_bonding >;

BOOST_FIXTURE_TEST_CASE( bonding_flag_in_response, legacy_bonding_response )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x00, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x01, 0x10, 0x00, 0x00
        }
    );
}

using legacy_mitm_response = test::legacy_security_manager< 23, bluetoe::require_man_in_the_middle_protection >;

BOOST_FIXTURE_TEST_CASE( mitm_flag_in_response, legacy_mitm_response )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x00, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x04, 0x10, 0x00, 0x00
        }
    );
}

using legacy_keypress_response = test::legacy_security_manager< 23, bluetoe::enable_keypress_notifications >;

BOOST_FIXTURE_TEST_CASE( keypress_flag_in_response, legacy_keypress_response )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x00, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x10, 0x10, 0x00, 0x00
        }
    );
}

using lesc_bonding_response = test::lesc_security_manager< 65, bluetoe::enable_bonding >;

BOOST_FIXTURE_TEST_CASE( lesc_bonding_flag_in_response, lesc_bonding_response )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x08, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x09, 0x10, 0x00, 0x00
        }
    );
}

using lesc_mitm_response = test::lesc_security_manager< 65, bluetoe::require_man_in_the_middle_protection >;

BOOST_FIXTURE_TEST_CASE( lesc_mitm_flag_in_response, lesc_mitm_response )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x08, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x0C, 0x10, 0x00, 0x00
        }
    );
}

using lesc_keypress_response = test::lesc_security_manager< 65, bluetoe::enable_keypress_notifications >;

BOOST_FIXTURE_TEST_CASE( lesc_keypress_flag_in_response, lesc_keypress_response )
{
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x08, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x18, 0x10, 0x00, 0x00
        }
    );
}

BOOST_AUTO_TEST_SUITE_END()
