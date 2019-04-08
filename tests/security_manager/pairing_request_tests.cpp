#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

template < class Manager, std::size_t MTU = 27 >
using sm = test::security_manager< Manager, MTU >;

BOOST_FIXTURE_TEST_SUITE( invalid_pairing_requests, sm< bluetoe::security_manager > )

    BOOST_AUTO_TEST_CASE( empty_request )
    {
        expected(
            {
            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE( invalid_opcode )
    {
        expected(
            {
                0x55
            },
            {
                0x05,           // Pairing Failed
                0x07            // Command Not Supported
            }
        );
    }

    BOOST_AUTO_TEST_CASE( no_parameters )
    {
        expected(
            {
                0x01            // Pairing Request
            },
            {
                0x05,           // Pairing Failed
                0x0A            // Invalid Parameters
            }
        );
    }

    BOOST_AUTO_TEST_CASE( invalid_IO_Capability )
    {
        expected(
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

    BOOST_AUTO_TEST_CASE( invalid_OOB_data_flag )
    {
        expected(
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

    BOOST_AUTO_TEST_CASE( invalid_AuthReq )
    {
        expected(
            {
                0x01,           // Pairing Request
                0x03,           // IO Capability NoInputNoOutput
                0x00,           // OOB data flag (data not present)
                0x80,           // AuthReq (RFU)
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

    BOOST_AUTO_TEST_CASE( invalid_Encryption_Key_Size )
    {
        expected(
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

    BOOST_AUTO_TEST_CASE( invalid_Encryption_Key_Size_II )
    {
        expected(
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

    BOOST_AUTO_TEST_CASE( invalid_Initiator_Key_Distribution )
    {
        expected(
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

    BOOST_AUTO_TEST_CASE( invalid_Responder_Key_Distribution )
    {
        expected(
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
