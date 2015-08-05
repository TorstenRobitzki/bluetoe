#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

BOOST_FIXTURE_TEST_CASE( respond_with_an_unknown_rsp, unconnected )
{
    this->respond_to( 37, valid_connection_request_pdu );
    add_connection_event_respond( {
        0x03, 0x01, 0xff
    } );
    add_connection_event_respond( {
        0x01, 0x00
    } );

    run();

    BOOST_REQUIRE_GE( connection_events().size(), 2 );
    auto event = connection_events()[ 1 ];

    BOOST_REQUIRE_EQUAL( event.transmitted_data.size(), 1 );
    auto const response = event.transmitted_data[ 0 ];

    BOOST_REQUIRE_EQUAL( response.size(), 4u );
    BOOST_CHECK_EQUAL( response[ 0 ] & 0x03, 0x03 );
    BOOST_CHECK_EQUAL( response[ 1 ], 2 );
    BOOST_CHECK_EQUAL( response[ 2 ], 0x07 );
    BOOST_CHECK_EQUAL( response[ 3 ], 0xff );
}

BOOST_FIXTURE_TEST_CASE( responding_in_feature_setup, connecting )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_version_ind, connecting )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_ping, connecting )
{
}

BOOST_FIXTURE_TEST_CASE( accept_termination, connecting )
{
}
