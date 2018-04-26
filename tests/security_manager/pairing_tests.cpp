#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

//#include <bluetoe/server.hpp>
#include <security_manager.hpp>
#include <test_servers.hpp>

template < class Manager, std::size_t MTU = 27 >
struct link_layer : Manager
{
    void expected(
        std::initializer_list< std::uint8_t > input,
        std::initializer_list< std::uint8_t > expected_output )
    {
        std::uint8_t buffer[ MTU ];
        std::size_t  size = MTU;

        this->l2cap_input( input.begin(), input.size(), &buffer[ 0 ], size );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            expected_output.begin(), expected_output.end(),
            &buffer[ 0 ], &buffer[ size ] );
    }
};

static const std::initializer_list< std::uint8_t > pairing_request = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

BOOST_FIXTURE_TEST_CASE( no_security_manager_no_pairing, link_layer< bluetoe::no_security_manager > )
{
    expected(
        pairing_request,
        {
            0x05, 0x05
        }
    );
}

BOOST_FIXTURE_TEST_CASE( by_default_no_oob_no_lesc, link_layer< bluetoe::security_manager > )
{
    expected(
        pairing_request,
        {
            0x02,   // response
            0x03,   // NoInputNoOutput
            0x00,   // OOB Authentication data not present
            0x40,   // Bonding, MITM = 0, SC = 0, Keypress = 0
            0x10,   // Maximum Encryption Key Size
            0x0f,   // LinkKey
            0x0f    // LinkKey
        }
    );
}

/**
 * SM/SLA/PROT/BV-02-C [SMP Time Out – IUT Responder]
 *
 * Verify that the IUT responder disconnects the link if pairing does not follow Pairing
 * Feature Exchange within 30 seconds after receiving Pairing Request command.
 */

/**
 * SM/SLA/JW/BV-02-C [Just Works IUT Responder – Success]
 *
 * Verify that the IUT is able to perform the Just Works pairing procedure correctly when acting as slave, responder.
 */
BOOST_FIXTURE_TEST_CASE( Just_Works_IUT_Responder__Success, link_layer< bluetoe::security_manager > )
{
    /*
    expected(
        {
            0x01,       // Pairing request
            0x03,       // NoInputNoOutput
            0x00,       // OOB Authentication data not present
            0x00,       // No Bonding, No MITM
            0x10,       // Maximum Encryption Key Size
            0x00,       // Initiator Key Distribution
            0x00        // Responder Key Distribution
        },
        {
            0x02,       // Pairing Response
            0x03,       // NoInputNoOutput
            0x00,       // OOB Authentication data not present
            0x00,       // No Bonding, MITM = 0, SC = 0, Keypress = 0
            0x10,       // Maximum Encryption Key Size
            0x00,       // Initiator Key Distribution
            0x00        // Responder Key Distribution
        }
    );
    */
}

/**
 * SM/SLA/JW/BI-03-C [Just Works IUT Responder – Handle AuthReq flag RFU correctly]
 *
 * Verify that the IUT is able to perform the Just Works pairing procedure when receiving additional
 * bits set in the AuthReq flag. Reserved For Future Use bits are correctly handled when acting as
 * slave, responder.
 */

/**
 * SM/SLA/JW/BI-02-C [Just Works, IUT Responder – Failure]
 *
 * Verify that the IUT handles just works pairing failure as responder correctly.
 */

/**
 * SM/SLA/PKE/BV-02-C (Passkey Entry, IUT Responder – Success)
 *
 * Verify that the IUT performs the Passkey Entry pairing procedure correctly as responder.
 */

/**
 * SM/SLA/PKE/BV-05-C [Passkey Entry, IUT Responder – Lower Tester has insufficient security for Passkey Entry]
 *
 * Verify that the IUT that supports the Passkey Entry pairing procedure as responder correctly
 * handles an initiator with insufficient security to result in an Authenticated key, yielding an
 * unauthenticated key.
 */

/**
 * SM/SLA/PKE/BI-03-C [Passkey Entry, IUT Responder – Failure on Initiator Side]
 *
 * Verify that the IUT handles the invalid passkey entry pairing procedure correctly as responder.
 */

/**
 * SM/SLA/OOB/BV-02-C [IUT Responder – Both sides have OOB data – Success]
 *
 * Verify that the IUT performs the OOB pairing procedure correctly as responder.
 */

/**
 * SM/SLA/OOB/BV-04-C [IUT Responder – Only IUT has OOB data – Success]
 *
 * Verify that the IUT performs the pairing procedure correctly as responder if only the
 * IUT has OOB data.
 */

/**
 * SM/SLA/OOB/BV-06-C [IUT Responder – Only Lower Tester has OOB data – Success]
 *
 * Verify that the IUT performs the pairing procedure correctly as responder if only the Lower Tester has OOB data.
 */

/**
 * SM/SLA/OOB/BV-08-C [IUT Responder – Only Lower Tester has OOB data – Lower Tester also supports Just Works]
 *
 * Verify that the IUT performs the pairing procedure correctly as responder if only the Lower Tester has OOB
 * data and supports the Just Works pairing method
 */

/**
 * SM/SLA/OOB/BV-10-C [IUT Responder – Only IUT has OOB data – Lower Tester also supports Just Works]
 *
 * Verify that the IUT performs the pairing procedure correctly as responder if only the IUT has
 * OOB data and the Lower Tester supports the Just Works pairing method.
 */

/**
 * SM/SLA/OOB/BI-02-C [IUT Responder – Both sides have different OOB data – Failure]
 *
 * Verify that the IUT responds to OOB pairing procedure and handles the failure correctly.
 */
