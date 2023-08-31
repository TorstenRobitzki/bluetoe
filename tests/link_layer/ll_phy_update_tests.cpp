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

    BOOST_AUTO_TEST_CASE( no_phy_update_feature_feature_mask )
    {
        BOOST_CHECK_EQUAL( supported_link_layer_features() & 0x100, 0 );
    }

    BOOST_AUTO_TEST_CASE( phy_request_not_supported )
    {
        ll_control_pdu(
            {
                0x16,                       // LL_PHY_REQ
                0x07,
                0x07
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x07,                       // LL_UNKNOWN_RSP
                0x16                        // LL_PHY_REQ
            }
        );
    }

    BOOST_AUTO_TEST_CASE( phy_update_not_supported )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x01,                       // -> 2Mbit
                0x01,                       // -> 2Mbit
                0x10, 0x00
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x07,                       // LL_UNKNOWN_RSP
                0x18                        // LL_PHY_UPDATE_IND
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

    BOOST_AUTO_TEST_CASE( phy_update_feature_feature_mask )
    {
        BOOST_CHECK_EQUAL( supported_link_layer_features() & 0x100, 0x100 );
    }

    BOOST_AUTO_TEST_CASE( phy_response )
    {
        ll_control_pdu(
            {
                0x16,                       // LL_PHY_REQ
                0x07,
                0x07
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x17,                       // LL_PHY_RSP
                0x03,                       // Sender prefers to use the LE 1M PHY (possibly among others)
                                            // Sender prefers to use the LE 2M PHY (possibly among others)
                0x03,                       // Sender prefers to use the LE 1M PHY (possibly among others)
                                            // Sender prefers to use the LE 2M PHY (possibly among others)
            }
        );
    }

    BOOST_AUTO_TEST_CASE( phy_request_wrong_size )
    {
        ll_control_pdu(
            {
                0x16,                       // LL_PHY_REQ
                0x07
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x07,                       // LL_UNKNOWN_RSP
                0x16                        // LL_PHY_REQ
            }
        );
    }

    BOOST_AUTO_TEST_CASE( phy_update )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x01,                       // Central -> Peripheral: 1MBit
                0x02,                       // Peripheral -> Central: 2MBit
                0x07, 0x00                  // Instance: 0x0007
            }
        );

        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();

        run( 8 );

        BOOST_CHECK_EQUAL( connection_events()[ 6 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 6 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );

        BOOST_CHECK_EQUAL( connection_events()[ 7 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 7 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_2m_phy );
    }

    BOOST_AUTO_TEST_CASE( phy_update_no_update )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x00,                       // Central -> Peripheral: no update
                0x00,                       // Peripheral -> Central: no update
                0x07, 0x00                  // Instance: 0x0007
            }
        );

        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();

        run( 8 );

        BOOST_CHECK_EQUAL( connection_events()[ 6 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 6 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );

        BOOST_CHECK_EQUAL( connection_events()[ 7 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 7 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
    }

    BOOST_AUTO_TEST_CASE( phy_p_to_c_update )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x00,                       // Central -> Peripheral: no update
                0x02,                       // Peripheral -> Central: 2 MBit
                0x07, 0x00                  // Instance: 0x0007
            }
        );

        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();
        ll_empty_pdu();

        run( 8 );

        BOOST_CHECK_EQUAL( connection_events()[ 6 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 6 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );

        BOOST_CHECK_EQUAL( connection_events()[ 7 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 7 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_2m_phy );
    }

    BOOST_AUTO_TEST_CASE( phy_update_wrong_size )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x01,                       // Central -> Peripheral: 1MBit
                0x02,                       // Peripheral -> Central: 2MBit
                0x07, 0x00,                 // Instance: 0x0007
                0xFF                        // Additional, not expected
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x07,                       // LL_UNKNOWN_RSP
                0x18                        // LL_PHY_UPDATE_IND
            }
        );
    }

    BOOST_AUTO_TEST_CASE( phy_update_multiple_c_to_p_bits )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x03,                       // Central -> Peripheral: 1MBit + 2MBit
                0x00,                       // Peripheral -> Central: no change
                0x07, 0x00                  // Instance: 0x0007
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x07,                       // LL_UNKNOWN_RSP
                0x18                        // LL_PHY_UPDATE_IND
            }
        );
    }

    BOOST_AUTO_TEST_CASE( phy_update_multiple_p_to_c_bits )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x00,                       // Central -> Peripheral: no change
                0x03,                       // Peripheral -> Central: 1MBit + 2MBit
                0x07, 0x00                  // Instance: 0x0007
            }
        );

        ll_empty_pdu();

        run( 5 );

        check_outgoing_ll_control_pdu(
            {
                0x07,                       // LL_UNKNOWN_RSP
                0x18                        // LL_PHY_UPDATE_IND
            }
        );
    }

BOOST_AUTO_TEST_SUITE_END()

struct with_2mbit_and_latency : unconnected_base_t<
    test::small_temperature_service,
    test::radio_with_2mbit >
{
    with_2mbit_and_latency()
    {
        this->respond_to( 37, {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval (30ms)
            0x0A, 0x00,                         // peripheral latency
            0x48, 0x00,                         // connection timeout (720ms)
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy (10 and 50ppm)
        } );
    }
};

BOOST_FIXTURE_TEST_SUITE( with_peripheral_latency, with_2mbit_and_latency )

    BOOST_AUTO_TEST_CASE( phy_p_to_c_update )
    {
        ll_control_pdu(
            {
                0x18,                       // LL_PHY_UPDATE_IND
                0x02,                       // Central -> Peripheral: 2 MBit
                0x02,                       // Peripheral -> Central: 2 MBit
                0x07, 0x00                  // Instance: 0x0007
            }
        );

        ll_empty_pdu();
        ll_empty_pdu();

        run( 3 );

        BOOST_CHECK_EQUAL( connection_events()[ 0 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 0 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_1m_phy );

        BOOST_CHECK_EQUAL( connection_events()[ 1 ].receiving_encoding   , bluetoe::link_layer::details::phy_ll_encoding::le_2m_phy );
        BOOST_CHECK_EQUAL( connection_events()[ 1 ].transmission_encoding, bluetoe::link_layer::details::phy_ll_encoding::le_2m_phy );
    }

BOOST_AUTO_TEST_SUITE_END()
