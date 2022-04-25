#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer.hpp>

#include "connected.hpp"

struct no_2mbit : unconnected_base_t<
    test::small_temperature_service,
    test::radio_no_2mbit >
{
    no_2mbit()
    {
        this->respond_to( 37, valid_connection_request_pdu );
    }
};

BOOST_FIXTURE_TEST_SUITE( no_support_by_hardware, no_2mbit )

    using test::X;

    BOOST_AUTO_TEST_CASE( no_phy_update_feature )
    {
        ll_control_pdu(
            {
                0x08,                    // LL_FEATURE_REQ
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            } );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x09,                   // LL_FEATURE_RSP
                X, 0x00, X, X,
                X, X, X, X
            }
        );
    }

BOOST_AUTO_TEST_SUITE_END()

struct with_2mbit : unconnected_base_t<
    test::small_temperature_service,
    test::radio_with_2mbit >
{
    with_2mbit()
    {
        this->respond_to( 37, valid_connection_request_pdu );
    }
};

BOOST_FIXTURE_TEST_SUITE( support_by_hardware, with_2mbit )

    using test::X;

    BOOST_AUTO_TEST_CASE( phy_update_feature )
    {
        ll_control_pdu(
            {
                0x08,                    // LL_FEATURE_REQ
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            } );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x09,                   // LL_FEATURE_RSP
                X, 0x01, X, X,
                X, X, X, X
            }
        );
    }

BOOST_AUTO_TEST_SUITE_END()
