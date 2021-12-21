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

BOOST_FIXTURE_TEST_CASE( not_in_idle_state, test::legacy_pairing_features_exchanged )
{
        expected(
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

