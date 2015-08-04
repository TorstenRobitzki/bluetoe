#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include "buffer_io.hpp"

/*
 * The connection interval is 30ms, the masters clock accuracy is 50ppm and the slave is configured with
 * the default of 500ppm (in sum 550ppm).
 * So the maximum derivation is 16µs
 */
BOOST_FIXTURE_TEST_CASE( smaller_window_after_connected, unconnected )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_event_respond( { 1, 0 } );

    run();

    BOOST_REQUIRE_GE( connection_events().size(), 1 );
    auto event = connection_events()[ 1 ];

    BOOST_CHECK_EQUAL( event.start_receive, bluetoe::link_layer::delta_time::usec( 30000 - 16 ) );
    BOOST_CHECK_EQUAL( event.end_receive, bluetoe::link_layer::delta_time::usec( 30000 + 16 ) );
}