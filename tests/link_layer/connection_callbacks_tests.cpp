#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <link_layer.hpp>

#include "connected.hpp"

struct only_connect_callback_t
{
    only_connect_callback_t()
        : connection_established_called( false )
    {
    }

    template < typename ConnectionData >
    void ll_connection_established(
        const bluetoe::link_layer::connection_details&      details,
        const bluetoe::link_layer::connection_addresses&    addresses,
        const ConnectionData& )
    {
        connection_established_called = true;
        reported_details              = details;
        reported_addresses            = addresses;
    }

    bool                                        connection_established_called;
    bluetoe::link_layer::connection_details     reported_details;
    bluetoe::link_layer::connection_addresses   reported_addresses;

} only_connect_callback;

struct only_changed_callback_t
{
    only_changed_callback_t()
        : only_changed_called( false )
    {
    }

    template < typename ConnectionData >
    void ll_connection_changed( const bluetoe::link_layer::connection_details& details, const ConnectionData& )
    {
        only_changed_called = true;
        reported_details    = details;
    }

    bool                                    only_changed_called;
    bluetoe::link_layer::connection_details reported_details;

} only_changed_callback;

struct only_disconnect_callback_t
{
    only_disconnect_callback_t()
        : only_disconnect_called( false )
    {
    }

    template < typename ConnectionData >
    void ll_connection_closed( const ConnectionData& )
    {
        only_disconnect_called = true;
    }

    bool only_disconnect_called;

} only_disconnect_callback;

struct reset_callbacks
{
    reset_callbacks()
    {
        only_connect_callback = only_connect_callback_t();
        only_changed_callback = only_changed_callback_t();
        only_disconnect_callback = only_disconnect_callback_t();
    }
};

template < class LinkLayer >
struct mixin_reset_callbacks : private reset_callbacks, public LinkLayer {};

using link_layer_only_connect_callback = mixin_reset_callbacks<
    unconnected_base<
        bluetoe::link_layer::connection_callbacks< only_connect_callback_t, only_connect_callback >,
        bluetoe::link_layer::sleep_clock_accuracy_ppm< 100u >,
        bluetoe::link_layer::static_address< 0xc0, 0x0f, 0x15, 0x08, 0x11, 0x47 >
    >
>;

using link_layer_only_changed_callback = mixin_reset_callbacks<
    unconnected_base<
        bluetoe::link_layer::connection_callbacks< only_changed_callback_t, only_changed_callback >,
        bluetoe::link_layer::sleep_clock_accuracy_ppm< 100u >
    >
>;

using link_layer_only_disconnect_callback = mixin_reset_callbacks<
    unconnected_base<
        bluetoe::link_layer::connection_callbacks< only_disconnect_callback_t, only_disconnect_callback >,
        bluetoe::link_layer::sleep_clock_accuracy_ppm< 100u >
    >
>;

BOOST_FIXTURE_TEST_CASE( connection_is_not_established_before_the_first_connection_event, link_layer_only_connect_callback )
{
    BOOST_CHECK( !only_connect_callback.connection_established_called );

    respond_to( 37, valid_connection_request_pdu );
    run();

    BOOST_CHECK( !only_connect_callback.connection_established_called );
}

BOOST_FIXTURE_TEST_CASE( connection_is_established_after_the_first_connection_event, link_layer_only_connect_callback )
{
    BOOST_CHECK( !only_connect_callback.connection_established_called );

    respond_to( 37, valid_connection_request_pdu );
    add_connection_event_respond( { 0, 1 } );
    run();

    BOOST_CHECK( only_connect_callback.connection_established_called );
}

BOOST_FIXTURE_TEST_CASE( connection_is_established_callback_called_only_once, link_layer_only_connect_callback )
{
    BOOST_CHECK( !only_connect_callback.connection_established_called );

    respond_to( 37, valid_connection_request_pdu );
    add_connection_event_respond( { 0, 1 } );
    add_connection_event_respond( { 0, 1 } );
    run();

    BOOST_CHECK( only_connect_callback.connection_established_called );
    only_connect_callback.connection_established_called = false;

    run();
    BOOST_CHECK( !only_connect_callback.connection_established_called );
}

namespace {
    bool equal( const bluetoe::link_layer::channel_map& lhs, const bluetoe::link_layer::channel_map& rhs )
    {
        for ( unsigned i = 0; i != bluetoe::link_layer::channel_map::max_number_of_data_channels; ++i )
        {
            if ( lhs.data_channel( i ) != rhs.data_channel( i ) )
                return false;
        }

        return true;
    }
}

BOOST_FIXTURE_TEST_CASE( connection_details_reported_when_connection_is_established, link_layer_only_connect_callback )
{
    respond_to( 37, {
        0xc5, 0x22,                         // header
        0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
        0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
        0x08, 0x81, 0xf6,                   // CRC Init
        0x03,                               // transmit window size
        0x0b, 0x00,                         // window offset
        0x18, 0x00,                         // interval (30ms)
        0x02, 0x00,                         // slave latency
        0x48, 0x05,                         // connection timeout
        0xf3, 0x5f, 0x1f, 0x7f, 0x1f,       // used channel map
        0xaa                                // hop increment and sleep clock accuracy (10 and 50ppm)
    } );
    add_connection_event_respond( { 0, 1 } );
    run( 2 );

    const auto reported_details = only_connect_callback.reported_details;

    static const std::uint8_t map_data[] = { 0xf3, 0x5f, 0x1f, 0x7f, 0x1f };
    bluetoe::link_layer::channel_map channels;
    channels.reset( &map_data[ 0 ], 10 );

    BOOST_CHECK( equal( reported_details.channels(), channels ) );
    BOOST_CHECK_EQUAL( reported_details.interval(), 0x18 );
    BOOST_CHECK_EQUAL( reported_details.latency(), 2 );
    BOOST_CHECK_EQUAL( reported_details.timeout(), 0x548 );
    BOOST_CHECK_EQUAL( reported_details.cumulated_sleep_clock_accuracy_ppm(), unsigned{ 50 + 100 } );
}

BOOST_FIXTURE_TEST_CASE( addresses_reported_when_connection_established, link_layer_only_connect_callback )
{
    respond_to( 37, {
        0x85, 0x22,                         // header
        0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (public)
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
        0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
        0x08, 0x81, 0xf6,                   // CRC Init
        0x03,                               // transmit window size
        0x0b, 0x00,                         // window offset
        0x18, 0x00,                         // interval (30ms)
        0x00, 0x00,                         // slave latency
        0x48, 0x00,                         // connection timeout (720ms)
        0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
        0xaa                                // hop increment and sleep clock accuracy (10 and 50ppm)
    } );

    add_connection_event_respond( { 0, 1 } );
    run();

    const auto reported_addresses = only_connect_callback.reported_addresses;
    BOOST_CHECK_EQUAL( reported_addresses.remote_address(), bluetoe::link_layer::public_device_address( { 0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48 } ) );
    BOOST_CHECK_EQUAL( reported_addresses.local_address(),  bluetoe::link_layer::random_device_address( { 0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0 } ) );
}

BOOST_FIXTURE_TEST_CASE( connection_update_not_called_by_default, link_layer_only_changed_callback )
{
    BOOST_CHECK( !only_changed_callback.only_changed_called );

    respond_to( 37, valid_connection_request_pdu );
    add_connection_event_respond( { 0, 1 } );
    run();

    BOOST_CHECK( !only_changed_callback.only_changed_called );
}

BOOST_FIXTURE_TEST_CASE( connection_update, link_layer_only_changed_callback )
{
    BOOST_CHECK( !only_changed_callback.only_changed_called );

    respond_to( 37, valid_connection_request_pdu );
    add_connection_update_request( 5, 6, 40, 1, 25, 2 );
    ll_empty_pdus( 120 );
    run( 3u );

    BOOST_CHECK( only_changed_callback.only_changed_called );
}

BOOST_FIXTURE_TEST_CASE( connection_details_reported_when_connection_is_updates, link_layer_only_changed_callback )
{
    respond_to( 37, valid_connection_request_pdu );
    add_connection_update_request( 5, 6, 40, 1, 25, 2 );
    ll_empty_pdus( 120 );
    run( 3u );

    static const std::uint8_t map_data[] = { 0xff, 0xff, 0xff, 0xff, 0x1f };
    bluetoe::link_layer::channel_map channels;
    channels.reset( &map_data[ 0 ], 10 );

    const auto reported_details = only_changed_callback.reported_details;
    BOOST_CHECK( equal( reported_details.channels(), channels ) );
    BOOST_CHECK_EQUAL( reported_details.interval(), 40 );
    BOOST_CHECK_EQUAL( reported_details.latency(), 1 );
    BOOST_CHECK_EQUAL( reported_details.timeout(), 25 );
    BOOST_CHECK_EQUAL( reported_details.cumulated_sleep_clock_accuracy_ppm(), unsigned{ 50 + 100 } );
}

BOOST_FIXTURE_TEST_CASE( never_connected, link_layer_only_disconnect_callback )
{
    run();

    BOOST_CHECK( !only_disconnect_callback.only_disconnect_called );

}

BOOST_FIXTURE_TEST_CASE( connection_not_lost, link_layer_only_disconnect_callback )
{
    BOOST_CHECK( !only_disconnect_callback.only_disconnect_called );

    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 120 );
    run( 3 );

    BOOST_CHECK( !only_disconnect_callback.only_disconnect_called );
}

BOOST_FIXTURE_TEST_CASE( connection_lost_by_timeout, link_layer_only_disconnect_callback )
{
    BOOST_CHECK( !only_disconnect_callback.only_disconnect_called );

    respond_to( 37, valid_connection_request_pdu );
    run( 2 );

    BOOST_CHECK( only_disconnect_callback.only_disconnect_called );
}

BOOST_FIXTURE_TEST_CASE( connection_lost_by_disconnect, link_layer_only_disconnect_callback )
{
    BOOST_CHECK( !only_disconnect_callback.only_disconnect_called );

    respond_to( 37, valid_connection_request_pdu );
    ll_empty_pdus( 3 );
    add_connection_event_respond(
        {
            0x03, 0x02,
            0x02, 0x12
        } );

    run( 2 );

    BOOST_CHECK( only_disconnect_callback.only_disconnect_called );
}
