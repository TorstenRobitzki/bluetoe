#include <bluetoe/link_layer/link_layer.hpp>
#include "../test_servers.hpp"
#include "test_radio.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>

#include <type_traits>

static const std::initializer_list< std::uint8_t > valid_connection_request_pdu =
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
};

const auto receive_and_transmit_data_filter = []( const test::schedule_data& data )
    {
        return data.receive_and_transmit;
    };

template < typename ... Options >
struct unconnected_base : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... >
{
    typedef bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... > base;

    void run()
    {
        small_temperature_service gatt_server_;
        base::run( gatt_server_ );
    }

    void check_connected( unsigned channel, const char* test ) const
    {
        // no advertising packages after the connect request
        bool last_advertising_message_found = false;

        this->check_scheduling(
            [&]( const test::schedule_data& data ) -> bool
            {
                const bool is_advertising = ( data.transmitted_data[ 0 ] & 0xf ) == 0;
                const bool result         = !last_advertising_message_found || !is_advertising;

                last_advertising_message_found = last_advertising_message_found || data.channel == channel;

                return result;
            },
            test
        );
    }

    void check_not_connected( const char* test ) const
    {
        this->check_scheduling(
            []( const test::schedule_data& data ) -> bool
            {
                return ( data.transmitted_data[ 0 ] & 0xf ) == 0 && !data.receive_and_transmit;
            },
            test
        );
    }

    void respond_with_connection_request( std::uint8_t window_size, std::uint16_t window_offset, std::uint16_t interval )
    {
        const std::vector< std::uint8_t > pdu =
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            window_size,                        // transmit window size
            static_cast< std::uint8_t >( window_offset & 0xff ),  // window offset
            static_cast< std::uint8_t >( window_offset >> 8 ),
            static_cast< std::uint8_t >( interval & 0xff ), // interval
            static_cast< std::uint8_t >( interval >> 8 ),
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        };

        this->respond_to( 37, pdu );
    }

};

struct unconnected : unconnected_base<> {};

template < typename ... Options >
struct connecting_base : unconnected_base< Options... >
{
    typedef bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... > base;

    connecting_base()
    {
        this->respond_to( 37, valid_connection_request_pdu );

        small_temperature_service gatt_server_;
        base::run( gatt_server_ );
    }
};

using connecting = connecting_base<>;

struct connected {};

typedef boost::mpl::list<
    std::integral_constant< unsigned, 37u >,
    std::integral_constant< unsigned, 38u >,
    std::integral_constant< unsigned, 39u > > advertising_channels;

BOOST_FIXTURE_TEST_SUITE( starting_unconnected_tests, unconnected )

/**
 * @test after a valid connection request, no further advertising packages are send.
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( connected_after_connection_request, Channel, advertising_channels )
{
    respond_to( Channel::value, valid_connection_request_pdu );

    run();

    check_connected( Channel::value, "connected_after_connection_request" );
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

BOOST_AUTO_TEST_CASE( no_connection_if_only_one_channel_is_used )
{
    respond_to(
        39,
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

    check_first_scheduling(
        receive_and_transmit_data_filter,
        []( const test::schedule_data& data )
        {
            return data.channel == ( 0 + 10 ) % 37;
        },
        "start_receiving_on_the_correct_channel"
    );

    // make sure, a at least one element within the filter was scheduled
    BOOST_CHECK_GE( count_data( receive_and_transmit_data_filter ), 1 );
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

    check_first_scheduling(
        receive_and_transmit_data_filter,
        []( const test::schedule_data& data )
        {
            return data.channel == ( ( 0 + 10 ) % 37 ) % 36 + 1;
        },
        "start_receiving_on_the_correct_channel"
    );
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
    check_first_scheduling(
        receive_and_transmit_data_filter,
        []( const test::schedule_data& data )
        {
            return data.transmision_time.usec() == 14992
                && data.end_receive.usec() == 18760;
        },
        "start_receiving_with_the_correct_window"
    );
}

/*
 * Second example, with a different configured sleep clock accuracy of 100ppm.
 * The master is announcing a sleep clock accuracy of 250 ppm. In sum: 350ppm.
 *
 * Start at:
 *   2001.25ms - 350ppm = 2001950µs
 * End at:
 *   ( 2001.25ms + 318.75 ) + 350ppm = 2320812µs
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
            0xff,                               // transmit window size = 318.75ms
            0x40, 0x06,                         // window offset 2 sec
            0x40, 0x06,                         // interval 2 sec
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0x2a                                // 1: sleep clock accuracy 151 ppm to 250 ppm
        }
    );

    run();

    check_scheduling(
        receive_and_transmit_data_filter,
        []( const test::schedule_data& data )
        {
            return data.transmision_time.usec() == 2001950
                && data.end_receive.usec() == 2320812;
        },
        "start_receiving_with_the_correct_window_II"
    );
}

BOOST_FIXTURE_TEST_CASE( first_scheduled_reply_should_be_an_empty_ll_data_pdu, connecting )
{
    check_scheduling(
        receive_and_transmit_data_filter,
        []( const test::schedule_data& data )
        {
            return data.transmitted_data.size() == 0x2
                && ( data.transmitted_data[ 0 ] & 0x3 ) == 0x1
                &&   data.transmitted_data[ 1 ] == 0;
        },
        "first_scheduled_reply_should_be_an_empty_ll_data_pdu"
    );
}

BOOST_FIXTURE_TEST_CASE( first_scheduled_reply_should_have_sn_and_nesn_zero, connecting )
{
    check_scheduling(
        receive_and_transmit_data_filter,
        []( const test::schedule_data& data )
        {
            return data.transmitted_data.size() == 0x2
                && ( data.transmitted_data[ 0 ] & 0x4 ) == 0
                && ( data.transmitted_data[ 1 ] & 0x8 ) == 0;
        },
        "first_scheduled_reply_should_be_an_empty_ll_data_pdu"
    );
}

/*
 * The first connection timeout is reached after 6 times the conenction interval is reached. Because the first
 * connection windows is already at least 1.25ms after the connection request, the sixed window would be already
 * after 6 * connectionInterval.
 */
BOOST_FIXTURE_TEST_CASE( there_should_be_5_receive_attempts_before_the_connecting_times_out, connecting )
{
    BOOST_CHECK_EQUAL( count_data(
        receive_and_transmit_data_filter
    ), 5 );
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

    check_scheduling(
        receive_and_transmit_data_filter,
        [&count]( const test::schedule_data& data )
        {
            unsigned start = 15000 + count * 30000;
            unsigned end   = start + 3750;
            ++count;

            start -= start * 550 / 1000000;
            end   += end * 550 / 1000000;

            return data.transmision_time.usec() == start
                && data.end_receive.usec() == end;
        },
        "window_widening_is_applied_with_every_receive_attempt"
    );
}

BOOST_FIXTURE_TEST_CASE( again_advertising_after_the_connection_timeout_was_reached, connecting )
{

}

BOOST_FIXTURE_TEST_CASE( link_layer_informs_host_about_established, unconnected )
{

}

BOOST_FIXTURE_TEST_CASE( connection_request_while_beeing_connected, connected )
{
}

