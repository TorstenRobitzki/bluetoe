#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "connected.hpp"
#include "../test_servers.hpp"

BOOST_FIXTURE_TEST_CASE( response_to_att_request, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_data_pdu(
        {
            0x03, 0x00,         // length
            0x04, 0x00,         // Channel
            0x02, 0x50, 0x00    // Exchange MTU Request
        } );
    ll_empty_pdu();

    run();

    auto response = connection_events().at( 1 ).transmitted_data.at( 0 );
    response[ 0 ] &= 0x03;

    static const std::uint8_t expected_response[] = {
        0x02, 0x07,             // ll header
        0x03, 0x00, 0x04, 0x00, // l2cap header
        0x03, 0x17, 0x00        // Exchange MTU Response
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::end( response ), std::begin( expected_response ), std::end( expected_response ) );
}