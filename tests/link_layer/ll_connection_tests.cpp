#include <bluetoe/link_layer/link_layer.hpp>
#include "../test_servers.hpp"
#include "test_radio.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>

#include <type_traits>

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
};

struct unconnected : unconnected_base<> {};

struct connecting {};

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
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

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

/*
 * At the start of a connection event, unmappedChannel shall be calculated using the following basic algorithm:
 *    unmappedChannel = (lastUnmappedChannel + hopIncrement) mod 37
 */
BOOST_FIXTURE_TEST_CASE( start_receiving_on_the_correct_channel, connecting )
{
}

/*
 * Assumed that the first unmappedChannel is not within the channel map, the
 */
BOOST_FIXTURE_TEST_CASE( start_receiving_on_a_remappped_channel, connecting )
{
}

BOOST_FIXTURE_TEST_CASE( start_receiving_with_the_correct_window, connecting )
{
}

BOOST_FIXTURE_TEST_CASE( connection_request_while_beeing_connected, connected )
{
}

