#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/options.hpp>

#include "connected.hpp"

struct only_connect_callback_t
{
    only_connect_callback_t()
        : connection_established_called( false )
    {
    }

    template < typename ConnectionData >
    void ll_connection_established( const bluetoe::link_layer::connection_details& details, const ConnectionData& connection )
    {
        connection_established_called = true;
    }

    bool connection_established_called;

} only_connect_callback;

struct only_changed_callback_t
{
    only_changed_callback_t()
        : only_changed_called( false )
    {
    }

    template < typename ConnectionData >
    void ll_connection_changed( const bluetoe::link_layer::connection_details& details, const ConnectionData& connection )
    {
        only_changed_called = true;
    }

    bool only_changed_called;

} only_changed_callback;

struct only_disconnect_callback_t
{
    only_disconnect_callback_t()
        : only_disconnect_called( false )
    {
    }

    template < typename ConnectionData >
    void ll_connection_closed( const ConnectionData& connection )
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
        bluetoe::link_layer::connection_callbacks< only_connect_callback_t, only_connect_callback >
    >
>;

using link_layer_only_changed_callback = mixin_reset_callbacks<
    unconnected_base<
        bluetoe::link_layer::connection_callbacks< only_changed_callback_t, only_changed_callback >
    >
>;

using link_layer_only_disconnect_callback = mixin_reset_callbacks<
    unconnected_base<
        bluetoe::link_layer::connection_callbacks< only_disconnect_callback_t, only_disconnect_callback >
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