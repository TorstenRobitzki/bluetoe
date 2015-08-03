#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/test/test_case_template.hpp>

#include <iostream>
#include "buffer_io.hpp"
#include "connected.hpp"

#include <type_traits>

BOOST_FIXTURE_TEST_SUITE( starting_unconnected_tests, unconnected )

/**
 * @test after a valid connection request, no further advertising packages are send.
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( connected_after_connection_request, Channel, advertising_channels )
{
    respond_to( Channel::value, valid_connection_request_pdu );

    run();

    BOOST_CHECK_GE( count_data(
        [&]( const test::advertising_data& data ) -> bool
        {
            bool is_advertising = ( data.transmitted_data[ 0 ] & 0xf ) == 0;
                 is_advertising = is_advertising && data.channel >= 37 && data.channel >= 40;

            return !is_advertising;
        } ), 1 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_connection_after_a_connection_request_with_wrong_length, Channel, advertising_channels )
{
    respond_to(
        Channel::value,
        {
            0xc5, 0x22,                         // header with wrong size
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f        // used channel map
        }
    );

    run();

    check_not_connected( "no_connection_after_a_connection_request_with_wrong_length" );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_connection_after_a_connection_request_with_wrong_length_header, Channel, advertising_channels )
{
    respond_to(
        Channel::value,
        {
            0xc5, 0x21,                         // header with wrong size
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run();

    check_not_connected( "no_connection_after_a_connection_request_with_wrong_length" );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_connection_after_a_connection_request_with_wrong_advertiser_address, Channel, advertising_channels )
{
    respond_to(
        Channel::value,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x18, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:18:11:47 (random) (not the correct address)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run();

    check_not_connected( "no_connection_after_a_connection_request_with_wrong_advertiser_address" );
}

typedef boost::mpl::list<
    std::integral_constant< unsigned, 0u >,
    std::integral_constant< unsigned, 4u >,
    std::integral_constant< unsigned, 17u > > invalid_hop_increments;

BOOST_AUTO_TEST_CASE_TEMPLATE( no_connection_if_hop_is_invalid, HopIncrement, invalid_hop_increments )
{
    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xa0 | HopIncrement::value          // hop increment and sleep clock accuracy
        }
    );

    run();

    check_not_connected( "no_connection_after_a_connection_request_with_wrong_advertiser_address" );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_connection_if_only_one_channel_is_used, Channel, advertising_channels )
{
    respond_to(
        Channel::value,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0x01, 0x00, 0x00, 0x00, 0x00,       // only channel 0
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run();

    check_not_connected( "no_connection_if_only_one_channel_is_used" );

}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_CASE( takes_the_give_access_address, unconnected )
{
    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run();

    BOOST_CHECK_EQUAL( access_address(), 0xaf9ab35a );
}

BOOST_FIXTURE_TEST_CASE( takes_the_give_initial_crc_value, unconnected )
{
    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run();

    BOOST_CHECK_EQUAL( crc_init(), 0xf68108 );
}

BOOST_FIXTURE_TEST_SUITE( starting_unconnected_tests, unconnected )

/*
 * At the start of a connection event, unmappedChannel shall be calculated using the following basic algorithm:
 *    unmappedChannel = (lastUnmappedChannel + hopIncrement) mod 37; for the first connection event lastUnmappedChannel is zero.
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( start_receiving_on_the_correct_channel, Channel, advertising_channels )
{
    respond_to( Channel::value, valid_connection_request_pdu );

    run();

    BOOST_REQUIRE( !connection_events().empty() );
    BOOST_CHECK_EQUAL( connection_events().front().channel, ( 0 + 10 ) % 37 );
}

BOOST_AUTO_TEST_SUITE_END()

/*
 * Assumed that the first unmappedChannel is not within the channel map, the remapped channel should be used
 */
BOOST_FIXTURE_TEST_CASE( start_receiving_on_a_remappped_channel, unconnected )
{
    respond_to(
        38,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xfb, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        } );

    run();

    BOOST_REQUIRE( !connection_events().empty() );
    BOOST_CHECK_EQUAL( connection_events().front().channel, ( ( 0 + 10 ) % 37 ) % 36 + 1 );
}

BOOST_FIXTURE_TEST_CASE( no_connection_if_transmit_window_is_larger_than_10ms, unconnected )
{
    respond_with_connection_request(
        0x09, // window_size
        0x0b, // window_offset
        0x18  // interval
    );

    run();

    check_not_connected( "no_connection_if_transmit_window_is_larger_than_10ms" );
}

BOOST_FIXTURE_TEST_CASE( no_connection_if_transmit_window_is_larger_than_connection_interval, unconnected )
{
    respond_with_connection_request(
        0x07, // window_size
        0x0b, // window_offset
        0x06  // interval
    );

    run();

    check_not_connected( "no_connection_if_transmit_window_is_larger_than_connection_interval" );
}

BOOST_FIXTURE_TEST_CASE( no_connection_if_transmit_window_offset_is_larger_than_connection_interval, unconnected )
{
    respond_with_connection_request(
        0x02, // window_size
        0x0b, // window_offset
        0x0a  // interval
    );

    run();

    check_not_connected( "no_connection_if_transmit_window_offset_is_larger_than_connection_interval" );
}

/*
 * The default devie sleep clock accuracy is 500ppm, the masters sca is 50ppm.
 * The last T0 was the reception of the connect request. The transmit window offset is
 * ( 11 + 1 ) * 1.25ms, the window size is 3 * 1.25 ms. So the window starts at:
 * 15ms - 15ms * 550ppm = 14992µs; the window ends at 18.75ms + 18.75ms * 550ppm = 18760µs
 */
BOOST_FIXTURE_TEST_CASE( start_receiving_with_the_correct_window, connecting )
{
    BOOST_REQUIRE( !connection_events().empty() );

    const auto& event = connection_events().front();

    BOOST_CHECK_EQUAL( event.start_receive.usec(), 14992 );
    BOOST_CHECK_EQUAL( event.end_receive.usec(), 18760 );
    BOOST_CHECK_EQUAL( event.connection_interval.usec(), 30000 );
}

/*
 * Second example, with a different configured sleep clock accuracy of 100ppm.
 * The master is announcing a sleep clock accuracy of 250 ppm. In sum: 350ppm.
 *
 * Start at:
 *   2000ms - 350ppm = 1999300µs
 * End at:
 *   ( 2000ms + 10ms ) + 350ppm = 2010703µs
 */
using local_device_with_100ppm = unconnected_base< bluetoe::link_layer::sleep_clock_accuracy_ppm< 100 > >;
BOOST_FIXTURE_TEST_CASE( start_receiving_with_the_correct_window_II, local_device_with_100ppm )
{
    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x08,                               // maximum transmit window size = 10ms
            0x3f, 0x06,                         // window offset 2 sec
            0x40, 0x06,                         // interval 2 sec
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0x2a                                // 1: sleep clock accuracy 151 ppm to 250 ppm
        }
    );

    run();

    BOOST_REQUIRE( !connection_events().empty() );

    const auto& event = connection_events().front();

    BOOST_CHECK_EQUAL( event.start_receive.usec(), 1999300 );
    BOOST_CHECK_EQUAL( event.end_receive.usec(), 2010703 );
    BOOST_CHECK_EQUAL( event.connection_interval.usec(), 2000000 );
}

/*
 * The first connection timeout is reached after 6 times the conenction interval is reached. Because the first
 * connection windows is already at least 1.25ms after the connection request, the sixed window would be already
 * after 6 * connectionInterval.
 */
BOOST_FIXTURE_TEST_CASE( there_should_be_5_receive_attempts_before_the_connecting_times_out, connecting )
{
    BOOST_CHECK_EQUAL( connection_events().size(), 5u );
}

/*
 * The default devie sleep clock accuracy is 500ppm, the masters sca is 50ppm.
 * The last T0 was the reception of the connect request. The transmit window offset is
 * ( 11 + 1 ) * 1.25ms, the window size is 3 * 1.25 ms. So the window starts at:
 * 15ms - 15ms * 550ppm = 14992µs; the window ends at 18.75ms + 18.75ms * 550ppm = 18760µs
 *
 * The conenction interval is 30ms
 */
BOOST_FIXTURE_TEST_CASE( window_widening_is_applied_with_every_receive_attempt, connecting )
{
    unsigned count = 0;

    for ( const auto& ev: connection_events() )
    {
        unsigned start = 15000 + count * 30000;
        unsigned end   = start + 3750;
        ++count;

        start -= start * 550 / 1000000;
        end   += end * 550 / 1000000;

        BOOST_CHECK_EQUAL( ev.start_receive.usec(), start );
        BOOST_CHECK_EQUAL( ev.end_receive.usec(), end );
    }
}

BOOST_FIXTURE_TEST_CASE( while_waiting_for_a_message_from_the_master_channels_are_hopped, connecting )
{
    std::vector< unsigned > expected_channels = { 10, 20, 30, 3, 13 };
    std::vector< unsigned > channels;

    for ( const auto& ev: connection_events() )
    {
        channels.push_back( ev.channel );
    }

    BOOST_CHECK_EQUAL_COLLECTIONS( expected_channels.begin(), expected_channels.end(), channels.begin(), channels.end() );
}

BOOST_FIXTURE_TEST_CASE( again_advertising_after_the_connection_timeout_was_reached, connecting )
{
    // there must be an advertising pdu after the last connection event attempt
    const auto last = connection_events().back().schedule_time;

    BOOST_CHECK_GE( count_data(
        [&last]( const test::advertising_data& adv )
        {
            return adv.schedule_time >= last;
        } ), 1 );
}