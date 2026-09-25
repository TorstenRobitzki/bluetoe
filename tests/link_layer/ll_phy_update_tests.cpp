#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer.hpp>

#include "connected.hpp"

using namespace test;
using bluetoe::link_layer::phy_ll_encoding::le_1m_phy;
using bluetoe::link_layer::phy_ll_encoding::le_2m_phy;

// the LE 2M PHY feature bit
static constexpr std::uint64_t le_2m_phy_feature = 0x100;

/*
 * The link layer under test, with or without a radio that supports the 2M PHY, and a check
 * of the PHYs a connection event was run on
 */
template < template < typename > class Radio >
struct phy_fixture : unconnected_base_t< small_temperature_service, Radio >
{
    void check_phys( std::size_t event, bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receiving, bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmitting ) const
    {
        BOOST_REQUIRE_GT( this->connection_events().size(), event );
        BOOST_CHECK_EQUAL( this->connection_events()[ event ].receiving_encoding, receiving );
        BOOST_CHECK_EQUAL( this->connection_events()[ event ].transmission_encoding, transmitting );
    }
};

struct no_2mbit : phy_fixture< radio_no_2mbit >
{
    no_2mbit()
    {
        respond_to( 37, valid_connection_request_pdu );
    }
};

struct with_2mbit : phy_fixture< radio_with_2mbit >
{
    with_2mbit()
    {
        respond_to( 37, valid_connection_request_pdu );
    }
};

BOOST_FIXTURE_TEST_SUITE( no_support_by_hardware, no_2mbit )

    BOOST_AUTO_TEST_CASE( no_phy_update_feature )
    {
        ll_control_pdu( ll_feature_req( 0 ) );
        ll_empty_pdu();

        run( 5 );

        // the LE 2M PHY bit clear, whatever else is claimed
        check_outgoing_ll_control_pdu( { 0x09, X, 0x00, X, X, X, X, X, X } );
    }

    BOOST_AUTO_TEST_CASE( no_phy_update_feature_feature_mask )
    {
        BOOST_CHECK_EQUAL( supported_link_layer_features() & le_2m_phy_feature, 0 );
    }

    BOOST_AUTO_TEST_CASE( phy_request_not_supported )
    {
        ll_control_pdu( ll_phy_req( phy::le_1m | phy::le_2m | phy::le_coded, phy::le_1m | phy::le_2m | phy::le_coded ) );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( ll_unknown_rsp( 0x16 ) );   // LL_PHY_REQ
    }

    BOOST_AUTO_TEST_CASE( phy_update_not_supported )
    {
        ll_control_pdu( ll_phy_update_ind( phy::le_1m, phy::le_1m, 0x10 ) );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( ll_unknown_rsp( 0x18 ) );   // LL_PHY_UPDATE_IND
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( support_by_hardware, with_2mbit )

    BOOST_AUTO_TEST_CASE( phy_update_feature )
    {
        ll_control_pdu( ll_feature_req( 0 ) );
        ll_empty_pdu();

        run( 5 );

        // the LE 2M PHY bit set, whatever else is claimed
        check_outgoing_ll_control_pdu( { 0x09, X, 0x01, X, X, X, X, X, X } );
    }

    BOOST_AUTO_TEST_CASE( phy_update_feature_feature_mask )
    {
        BOOST_CHECK_EQUAL( supported_link_layer_features() & le_2m_phy_feature, le_2m_phy_feature );
    }

    // asked for any PHY, the link layer prefers the 1M and the 2M PHY in both directions
    BOOST_AUTO_TEST_CASE( phy_response )
    {
        ll_control_pdu( {
            0x16,                       // LL_PHY_REQ
            0x07,                       // TX_PHYS: LE 1M, LE 2M and LE Coded
            0x07                        // RX_PHYS: LE 1M, LE 2M and LE Coded
        } );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( {
            0x17,                       // LL_PHY_RSP
            0x03,                       // TX_PHYS: LE 1M and LE 2M
            0x03                        // RX_PHYS: LE 1M and LE 2M
        } );
    }

    BOOST_AUTO_TEST_CASE( phy_request_wrong_size )
    {
        // an LL_PHY_REQ with a byte too few
        ll_control_pdu( { 0x16, 0x07 } );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( ll_unknown_rsp( 0x16 ) );   // LL_PHY_REQ
    }

    BOOST_AUTO_TEST_CASE( phy_update )
    {
        ll_control_pdu( {
            0x18,                       // LL_PHY_UPDATE_IND
            0x01,                       // C_TO_P_PHY: LE 1M
            0x02,                       // P_TO_C_PHY: LE 2M
            0x07, 0x00                  // Instant
        } );
        ll_empty_pdus( 7 );

        run( 8 );

        check_phys( 6, le_1m_phy, le_1m_phy );
        check_phys( 7, le_1m_phy, le_2m_phy );
    }

    BOOST_AUTO_TEST_CASE( phy_update_no_update )
    {
        ll_control_pdu( ll_phy_update_ind( phy::none, phy::none, 7 ) );
        ll_empty_pdus( 7 );

        run( 8 );

        check_phys( 6, le_1m_phy, le_1m_phy );
        check_phys( 7, le_1m_phy, le_1m_phy );
    }

    BOOST_AUTO_TEST_CASE( phy_p_to_c_update )
    {
        ll_control_pdu( ll_phy_update_ind( phy::none, phy::le_2m, 7 ) );
        ll_empty_pdus( 7 );

        run( 8 );

        check_phys( 6, le_1m_phy, le_1m_phy );
        check_phys( 7, le_1m_phy, le_2m_phy );
    }

    BOOST_AUTO_TEST_CASE( phy_update_wrong_size )
    {
        // an LL_PHY_UPDATE_IND with a byte too many
        ll_control_pdu( { 0x18, 0x01, 0x02, 0x07, 0x00, 0xff } );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( ll_unknown_rsp( 0x18 ) );   // LL_PHY_UPDATE_IND
    }

    BOOST_AUTO_TEST_CASE( phy_update_multiple_c_to_p_bits )
    {
        ll_control_pdu( ll_phy_update_ind( phy::le_1m | phy::le_2m, phy::none, 7 ) );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( ll_unknown_rsp( 0x18 ) );   // LL_PHY_UPDATE_IND
    }

    BOOST_AUTO_TEST_CASE( phy_update_multiple_p_to_c_bits )
    {
        ll_control_pdu( ll_phy_update_ind( phy::none, phy::le_1m | phy::le_2m, 7 ) );
        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu( ll_unknown_rsp( 0x18 ) );   // LL_PHY_UPDATE_IND
    }

BOOST_AUTO_TEST_SUITE_END()

struct with_2mbit_and_latency : phy_fixture< radio_with_2mbit >
{
    with_2mbit_and_latency()
    {
        respond_to( 37, connect_ind( { .latency = 10 } ) );
    }
};

BOOST_FIXTURE_TEST_SUITE( with_peripheral_latency, with_2mbit_and_latency )

    // with a latency of 10 the event at the instant is the second one the peripheral listens to
    BOOST_AUTO_TEST_CASE( phy_p_to_c_update )
    {
        ll_control_pdu( ll_phy_update_ind( phy::le_2m, phy::le_2m, 7 ) );
        ll_empty_pdus( 2 );

        run( 3 );

        check_phys( 0, le_1m_phy, le_1m_phy );
        check_phys( 1, le_2m_phy, le_2m_phy );
    }

BOOST_AUTO_TEST_SUITE_END()
