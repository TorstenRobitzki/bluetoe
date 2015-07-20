#include <bluetoe/link_layer/link_layer.hpp>
#include "../test_servers.hpp"
#include "test_radio.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>

#include <type_traits>
#include <iostream>

template < typename ... Options >
struct unconnected_base : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... >
{
    typedef bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... > base;

    void run()
    {
        small_temperature_service gatt_server_;
        base::run( gatt_server_ );
    }
};

struct unconnected : unconnected_base<> {};

struct connected {};

typedef boost::mpl::list<
    std::integral_constant< unsigned, 37u >/*,
    std::integral_constant< unsigned, 38u >,
    std::integral_constant< unsigned, 39u > */> advertising_channels;

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
            0x4e, 0x0f, 0xe7, 0xe5, 0x0f, 0xc0, // AdvA:  c0:0f:e5:e7:0f:4e (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa,                               // hop increment and sleep clock accuracy
            0xb2, 0x95, 0x69                    // check sum
        }
    );

    run();

    // no advertising packages after the connect request
    bool last_advertising_message_found = false;

    check_scheduling(
        [&last_advertising_message_found]( const test::schedule_data& data ) -> bool
        {
            const bool result = !last_advertising_message_found;
            last_advertising_message_found = last_advertising_message_found || data.channel == Channel::value;

            return result;
        },
        "connected_after_connection_request"
    );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_CASE( no_connection_after_a_broken_connection_request, unconnected )
{
}

/**
 * @test no connection is established when the connection request doesn't contain the devices address
 */
BOOST_FIXTURE_TEST_CASE( connection_request_from_wrong_address, unconnected )
{
}

BOOST_FIXTURE_TEST_CASE( connection_request_while_beeing_connected, connected )
{
}

