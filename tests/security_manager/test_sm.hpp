#ifndef BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP
#define BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP

namespace test {

    template < class Manager, std::size_t MTU = 27 >
    struct security_manager : Manager
    {
        void expected(
            std::initializer_list< std::uint8_t > input,
            std::initializer_list< std::uint8_t > expected_output )
        {
            std::uint8_t buffer[ MTU ];
            std::size_t  size = MTU;

            this->l2cap_input( input.begin(), input.size(), &buffer[ 0 ], size, *this );

            BOOST_CHECK_EQUAL_COLLECTIONS(
                expected_output.begin(), expected_output.end(),
                &buffer[ 0 ], &buffer[ size ] );
        }
    };

    struct pairing_features_exchanged : security_manager< bluetoe::security_manager, 27 >
    {
        pairing_features_exchanged()
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
                    0x02,   // response
                    0x03,   // NoInputNoOutput
                    0x00,   // OOB Authentication data not present
                    0x00,   // Bonding, MITM = 0, SC = 0, Keypress = 0
                    0x10,   // Maximum Encryption Key Size
                    0x00,   // LinkKey
                    0x00    // LinkKey
                }
            );
        }
    };

}

#endif
