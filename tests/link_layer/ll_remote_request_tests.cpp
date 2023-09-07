#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

struct fixture : unconnected_base< >
{
    fixture()
    {
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
