#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

struct callbacks_t
{
    callbacks_t()
        : connection_closed( false )
        , version_received( false )
        , connection_changed( false )
    {
    }

    template < class ConnectionData >
    void ll_connection_closed( std::uint8_t reason, ConnectionData& )
    {
        connection_closed = true;
        closed_reason = reason;
    }

    template < class ConnectionData >
    void ll_version( std::uint8_t version, std::uint16_t company, std::uint16_t subversion, const ConnectionData& )
    {
        version_received = true;
        version_version  = version;
        version_company  = company;
        version_subversion = subversion;
    }

    template < typename ConnectionData >
    void ll_connection_changed(
        const bluetoe::link_layer::connection_details& details, ConnectionData& )
    {
        connection_changed = true;
        changed_details    = details;
    }

    bool            connection_closed;
    std::uint8_t    closed_reason;

    bool            version_received;
    std::uint8_t    version_version;
    std::uint16_t   version_company;
    std::uint16_t   version_subversion;

    bool            connection_changed;
    bluetoe::link_layer::connection_details changed_details;

} callbacks;

struct fixture : unconnected_base<
    bluetoe::link_layer::connection_callbacks< callbacks_t, callbacks > >
{
    fixture()
    {
        callbacks = callbacks_t();
        this->respond_to( 37, valid_connection_request_pdu );
    }
};

using test::X;

BOOST_FIXTURE_TEST_CASE( request_to_2mbit, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK( phy_update_request_to_2mbit() );
        BOOST_CHECK( !phy_update_request_to_2mbit() );
    });
    ll_empty_pdus( 4 );

    run( 5 );

    check_outgoing_ll_control_pdu(
        {
            0x16,                   // LL_PHY_REQ
            0x02,                   // LE 2M PHY
            0x02                    // LE 2M PHY
        }
    );
}

BOOST_FIXTURE_TEST_CASE( request_to_1_2mbit, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK(
            phy_update_request(
                bluetoe::link_layer::phy_ll_encoding::le_1m_phy,
                bluetoe::link_layer::phy_ll_encoding::le_2m_phy ) );
    });
    ll_empty_pdus( 4 );

    run( 5 );

    check_outgoing_ll_control_pdu(
        {
            0x16,                   // LL_PHY_REQ
            0x01,                   // LE 1M PHY
            0x02                    // LE 2M PHY
        }
    );
}

BOOST_FIXTURE_TEST_CASE( request_to_2_xmbit, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK(
            phy_update_request(
                bluetoe::link_layer::phy_ll_encoding::le_2m_phy,
                bluetoe::link_layer::phy_ll_encoding::le_unchanged_coding ) );
    });
    ll_empty_pdus( 4 );

    run( 5 );

    check_outgoing_ll_control_pdu(
        {
            0x16,                   // LL_PHY_REQ
            0x02,                   // LE 2M PHY
            0x00                    // unchanged
        }
    );
}

BOOST_FIXTURE_TEST_CASE( request_to_remote_version, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK( remote_versions_request() );
        BOOST_CHECK( !remote_versions_request() );
    });
    ll_empty_pdus( 4 );

    run( 5 );

    check_outgoing_ll_control_pdu(
        {
            0x0C,                   // LL_VERSION_IND
            0x09,                   // Version
            0x69, 0x02,             // Company_Identifier
            0x00, 0x00              // Subversion
        }
    );
}

BOOST_FIXTURE_TEST_CASE( request_to_remote_version_pending_till_response, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK( remote_versions_request() );
        BOOST_CHECK( !remote_versions_request() );
    });
    ll_empty_pdus( 4 );
    ll_function_call( [this](){
        // even after sending out the LL_VERSION_IND PDU, any procedure
        // is looked due to the still pending procedure. Procedure
        // is still pending, as no response was received yet.
        BOOST_CHECK( !remote_versions_request() );
    });
    ll_control_pdu(
        {
            0x0C,                   // LL_VERSION_IND
            0x08,                   // Version
            0x47, 0x11,             // Company_Identifier
            0x08, 0x15              // Subversion
        }
    );

    run( 5 );

    check_outgoing_ll_control_pdu(
        {
            0x0C,                   // LL_VERSION_IND
            0x09,                   // Version
            0x69, 0x02,             // Company_Identifier
            0x00, 0x00              // Subversion
        }
    );

    // Now again, we can request the remote version
    ll_function_call( [this](){
        BOOST_CHECK( remote_versions_request() );
    });
    ll_empty_pdus( 4 );

    run( 5 );

    check_outgoing_ll_control_pdu(
        {
            0x0C,                   // LL_VERSION_IND
            0x09,                   // Version
            0x69, 0x02,             // Company_Identifier
            0x00, 0x00              // Subversion
        }
    );
}

/**
 * "If the procedure response timeout timer reaches 40 seconds, the ACL connection
 * is considered lost (see Section 4.5.12). The Link Layer exits the Connection
 * state and shall transition to the Standby state. The Host shall be notified of
 * the loss of connection."
 *
 * LL/CON/PER/BI-05-C
 */
BOOST_FIXTURE_TEST_CASE( request_to_remote_version_timeout, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK( remote_versions_request() );
    });
    // connection interval is 30ms / timeout 4000ms = 1333,3
    ll_empty_pdus( 1330 );
    ll_function_call( [this](){
        BOOST_CHECK( !callbacks.connection_closed );
        BOOST_CHECK( !remote_versions_request() );
    });
    ll_empty_pdus( 3 );
    ll_function_call( [](){
        BOOST_CHECK( callbacks.connection_closed );
    });

    run( 1500 );

    BOOST_CHECK( callbacks.connection_closed );
    BOOST_CHECK_EQUAL( callbacks.closed_reason, 0x22 );
}

/**
 * LL/CON/PER/BV-24-C
 */
BOOST_FIXTURE_TEST_CASE( Initiating_Connection_Parameter_Request__Accept, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK( initiating_connection_parameter_request( 6, 6, 0, 300 ) );
        BOOST_CHECK( !initiating_connection_parameter_request( 6, 6, 0, 300 ) );
    });
    ll_empty_pdus( 4 );

    ll_control_pdu(
        {
            0x00,                   // LL_CONNECTION_UPDATE_IND
            0x01,                   // WinSize
            0x06, 0x00,             // WinOffset
            0x06, 0x00,             // Interval
            0x00, 0x00,             // Latency
            0x2c, 0x01,             // Timeout
            0x0A, 0x00              // Instant
        }
    );
    ll_empty_pdus( 20 );

    run( 25 );

    check_outgoing_ll_control_pdu(
        {
            0x0F,                   // LL_CONNECTION_PARAM_REQ
            0x06, 0x00,             // Interval_Min
            0x06, 0x00,             // Interval_Max
            0x00, 0x00,             // Latency
            0x2c, 0x01,             // Timeout
            0x00,                   // PreferredPeriodicity
            0x00, 0x00,             // ReferenceConnEventCount
            0xff, 0xff,             // Offset0
            0xff, 0xff,             // Offset1
            0xff, 0xff,             // Offset2
            0xff, 0xff,             // Offset3
            0xff, 0xff,             // Offset4
            0xff, 0xff,             // Offset5
        }
    );

    BOOST_CHECK( callbacks.connection_changed );
    BOOST_CHECK_EQUAL( callbacks.changed_details.interval(), 0x0006 );
    BOOST_CHECK_EQUAL( callbacks.changed_details.latency(), 0x0000 );
    BOOST_CHECK_EQUAL( callbacks.changed_details.timeout(), 0x012c );

    // Finally, a new update request will be accepted again
    BOOST_CHECK( initiating_connection_parameter_request( 6, 7, 1, 300 ) );
}

/**
 * LL/CON/PER/BI-07-C
 */
BOOST_FIXTURE_TEST_CASE( Initiating_Connection_Parameter_Request__Timeout, fixture )
{
    ll_empty_pdus( 1 );
    ll_function_call( [this](){
        BOOST_CHECK( initiating_connection_parameter_request( 6, 6, 0, 300 ) );
    });
    // connection interval is 30ms / timeout 4000ms = 1333,3
    ll_empty_pdus( 1330 );
    ll_function_call( [this](){
        BOOST_CHECK( !callbacks.connection_closed );
        BOOST_CHECK( !initiating_connection_parameter_request( 6, 6, 0, 300 ) );
    });
    ll_empty_pdus( 3 );
    ll_function_call( [](){
        BOOST_CHECK( callbacks.connection_closed );
    });

    run( 1500 );

    BOOST_CHECK( callbacks.connection_closed );
    BOOST_CHECK_EQUAL( callbacks.closed_reason, 0x22 );
}
