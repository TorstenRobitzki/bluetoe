#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

BOOST_FIXTURE_TEST_CASE( respond_with_an_unknown_rsp, unconnected )
{
    check_single_ll_control_pdu(
        { 0x03, 0x01, 0xff },
        {
            0x03, 0x02,
            0x07, 0xff
        },
        "respond_with_an_unknown_rsp"
    );
}

BOOST_FIXTURE_TEST_CASE( responding_in_feature_setup, unconnected )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_version_ind, unconnected )
{
    check_single_ll_control_pdu(
        {
            0x03, 0x06,
            0x0C,               // LL_VERSION_IND
            0x08,               // VersNr = Core Specification 4.2
            0x00, 0x02,         // CompId
            0x00, 0x00          // SubVersNr
        },
        {
            0x03, 0x06,
            0x0C,               // LL_VERSION_IND
            0x08,               // VersNr = Core Specification 4.2
            0x69, 0x02,         // CompId
            0x00, 0x00          // SubVersNr
        },
        "respond_to_a_version_ind"
    );
}

BOOST_FIXTURE_TEST_CASE( no_second_respond_to_a_version_request, unconnected )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_ping, unconnected )
{
}

BOOST_FIXTURE_TEST_CASE( accept_termination, unconnected )
{
}
