#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include "buffer_io.hpp"

struct only_one_pdu_from_master : unconnected
{
    only_one_pdu_from_master()
    {
        respond_to( 37, valid_connection_request_pdu );
        add_connection_event_respond( { 1, 0 } );

        run();
    }

};

/*
 * The connection interval is 30ms, the masters clock accuracy is 50ppm and the slave is configured with
 * the default of 500ppm (in sum 550ppm).
 * So the maximum derivation is 16µs
 */
BOOST_FIXTURE_TEST_CASE( smaller_window_after_connected, only_one_pdu_from_master )
{
    BOOST_REQUIRE_GE( connection_events().size(), 1 );
    auto event = connection_events()[ 1 ];

    BOOST_CHECK_EQUAL( event.start_receive, bluetoe::link_layer::delta_time::usec( 30000 - 16 ) );
    BOOST_CHECK_EQUAL( event.end_receive, bluetoe::link_layer::delta_time::usec( 30000 + 16 ) );
}

/*
 * For the second connection event, the derivation from the 2*30ms is 33µs
 */
BOOST_FIXTURE_TEST_CASE( window_size_is_increasing_with_connection_event_timeouts, only_one_pdu_from_master )
{
    BOOST_REQUIRE_GE( connection_events().size(), 2 );
    auto event = connection_events()[ 2 ];

    BOOST_CHECK_EQUAL( event.start_receive, bluetoe::link_layer::delta_time::usec( 60000 - 33 ) );
    BOOST_CHECK_EQUAL( event.end_receive, bluetoe::link_layer::delta_time::usec( 60000 + 33 ) );

}
/*
 * Once the link layer received a PDU from the master, the supervision timeout is in charge
 * In this example, the timeout is 720ms, the connection interval is 30ms, so the timeout is
 * reached after 24 connection intervals (that's the 25th schedule request; plus the first one after the con request).
 */
BOOST_FIXTURE_TEST_CASE( supervision_timeout_is_in_charge, only_one_pdu_from_master )
{
    BOOST_CHECK_EQUAL( connection_events().size(), 26u );
}

