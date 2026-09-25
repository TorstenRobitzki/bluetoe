#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "buffer_io.hpp"
#include "connected.hpp"

#include <bluetoe/link_layer.hpp>
#include <bluetoe/l2cap_signaling_channel.hpp>


using namespace test;

// what the peripheral asks for in these tests, in one place: an interval of 10 to 20,
// a latency of 3 and a timeout of 160
static const connection_parameter_request requested = { .min_interval = 10, .max_interval = 20, .latency = 3, .timeout = 2 * 20 * 4 };

// the versions of the Core Specification and the feature a LL_FEATURE_REQ names
static constexpr std::uint8_t  core_4_0                               = 0x06;
static constexpr std::uint16_t some_company                           = 0x0200;
static constexpr std::uint64_t connection_parameters_request_procedure = 0x02;

/*
 * To request the update of the connection parameters, a peripheral / link layer peripheral
 * has two options:
 * - LL Connection Parameters Request Procedure (ll)
 * - L2CAP Connection Parameter Update Request (l2cap)
 *
 * Where the standard prefers the the former, if both, peripheral and central supports
 * that procedure.
 *
 * So, to pick one of them, its crucial to know whether the central supports the
 * LL Connection Parameters Request Procedure.
 *
 * Lets start by assuming, that the central implements the LL Connection Parameters Request Procedure.
 * If a LL_VERSION_IND is received from the central indicating, that the centrals link layer version
 * is 4.0 (or less), then the central does not implement LL Connection Parameters Request Procedure.
 * If a LL_FEATURE_REQ PDU is received, it's clear whether the central implements the
 * LL Connection Parameters Request Procedure or not.
 *
 * LL Connection Parameters Request Procedure should be optional, so if the peripheral votes to not implement
 * LL Connection Parameters Request Procedure, than we should default to l2cap.
 */

struct link_layer_with_signaling_channel : unconnected_base< bluetoe::l2cap::signaling_channel<>, test::buffer_sizes >
{
    link_layer_with_signaling_channel()
    {
        respond_to( 37, valid_connection_request_pdu );
    }

    bool request_the_parameters()
    {
        return connection_parameter_update_request( requested.min_interval, requested.max_interval, requested.latency, requested.timeout );
    }
};

BOOST_FIXTURE_TEST_CASE( if_central_protocol_version_is_unknown_try_ll, link_layer_with_signaling_channel )
{
    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_req( requested ) );
}

BOOST_FIXTURE_TEST_CASE( if_central_protocol_version_is_40_use_l2cap, link_layer_with_signaling_channel )
{
    ll_control_pdu( ll_version_ind( core_4_0, some_company, 0 ) );

    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_l2cap_pdu( l2cap_connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
}

BOOST_FIXTURE_TEST_CASE( if_centrals_features_dont_contain_ll_use_l2cap, link_layer_with_signaling_channel )
{
    ll_control_pdu( ll_feature_req( 0 ) );

    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_l2cap_pdu( l2cap_connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
}

BOOST_FIXTURE_TEST_CASE( if_centrals_features_contain_ll_use_ll, link_layer_with_signaling_channel )
{
    ll_control_pdu( ll_feature_req( connection_parameters_request_procedure ) );

    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_req( requested ) );
}

BOOST_FIXTURE_TEST_CASE( if_ll_doesn_work_fallback_to_l2cap, link_layer_with_signaling_channel )
{
    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    ll_control_pdu( ll_unknown_rsp( 0x0f ) );      // LL_CONNECTION_PARAM_REQ unknown

    ll_empty_pdus(3);

    run( 5 );

    // first tried with LL_CONNECTION_PARAM_REQ
    check_outgoing_ll_control_pdu( ll_connection_param_req( requested ) );

    // as fallback, try l2cap
    check_outgoing_l2cap_pdu( l2cap_connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
}

BOOST_FIXTURE_TEST_CASE( if_ll_was_rejected_fallback_to_l2cap, link_layer_with_signaling_channel )
{
    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    ll_control_pdu( ll_reject_ind( 0x1a ) );       // Unsupported Remote/LMP Feature

    ll_empty_pdus(3);

    run( 5 );

    // first tried with LL_CONNECTION_PARAM_REQ
    check_outgoing_ll_control_pdu( ll_connection_param_req( requested ) );

    // as fallback, try l2cap
    check_outgoing_l2cap_pdu( l2cap_connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
}

BOOST_FIXTURE_TEST_CASE( if_ll_was_rejected_ext_fallback_to_l2cap, link_layer_with_signaling_channel )
{
    ll_function_call( [&]{ BOOST_REQUIRE( request_the_parameters() ); } );

    ll_empty_pdus(3);

    ll_control_pdu( ll_reject_ext_ind( 0x0f, 0x1a ) );  // LL_CONNECTION_PARAM_REQ: Unsupported Remote/LMP Feature

    ll_empty_pdus(3);

    run( 5 );

    // first tried with LL_CONNECTION_PARAM_REQ
    check_outgoing_ll_control_pdu( ll_connection_param_req( requested ) );

    // as fallback, try l2cap
    check_outgoing_l2cap_pdu( l2cap_connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
}

// what the central asks for in these tests: the same as the peripheral, with two offsets
static const std::array< std::uint16_t, 6 > two_offsets = { 2, 4, 0xffff, 0xffff, 0xffff, 0xffff };

static const connection_parameter_request central_request = {
    .min_interval = 10, .max_interval = 20, .latency = 3, .timeout = 2 * 20 * 4, .offsets = two_offsets };

BOOST_FIXTURE_TEST_CASE( copy_data_from_request, link_layer_with_signaling_channel )
{
    ll_control_pdu( ll_connection_param_req( central_request ) );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_rsp( central_request ) );
}

using desired_connection_parameters = bluetoe::link_layer::desired_connection_parameters<
    11, 19,
    2, 20,
    1*20*4, 2*20*4
>;

struct link_layer_with_desired_connection_parameters : unconnected_base< bluetoe::l2cap::signaling_channel<>, test::buffer_sizes, desired_connection_parameters >
{
    link_layer_with_desired_connection_parameters()
    {
        respond_to( 37, valid_connection_request_pdu );
    }
};

// the peripheral's answer to the central's request, within its desired interval of 11 to 19
static const connection_parameter_request desired_answer = {
    .min_interval = 11, .max_interval = 19, .latency = 3, .timeout = 2 * 20 * 4, .offsets = two_offsets };

BOOST_FIXTURE_TEST_CASE( respond_with_desired_parameters, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( ll_connection_param_req( central_request ) );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_rsp( desired_answer ) );
}

BOOST_FIXTURE_TEST_CASE( requested_interval_outside_of_desired_values, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( ll_connection_param_req( {
        .min_interval = 20, .max_interval = 30, .latency = 3, .timeout = 2 * 20 * 4, .offsets = two_offsets } ) );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_rsp( desired_answer ) );
}

BOOST_FIXTURE_TEST_CASE( requested_latency_outside_of_desired_values, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( ll_connection_param_req( with( central_request, &connection_parameter_request::latency, 0 ) ) );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_rsp( with( desired_answer, &connection_parameter_request::latency, 11 ) ) );
}

BOOST_FIXTURE_TEST_CASE( requested_timeout_outside_of_desired_values, link_layer_with_desired_connection_parameters )
{
    // a timeout of 496, beyond the desired 160
    ll_control_pdu( ll_connection_param_req( with( central_request, &connection_parameter_request::timeout, 496 ) ) );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_rsp( with( desired_answer, &connection_parameter_request::timeout, 120 ) ) );
}

struct connection_parameter_update_cb_t
{
public:
    connection_parameter_update_cb_t()
        : remote_connection_parameter_request_received( false )
    {
    }

    void ll_remote_connection_parameter_request(
        std::uint16_t interval_min,
        std::uint16_t interval_max,
        std::uint16_t latency,
        std::uint16_t timeout )
    {
        remote_connection_parameter_request_received = true;

        requested_interval_min  = interval_min;
        requested_interval_max  = interval_max;
        requested_latency       = latency;
        requested_timeout       = timeout;
    }

    bool remote_connection_parameter_request_received;
    std::uint16_t requested_interval_min;
    std::uint16_t requested_interval_max;
    std::uint16_t requested_latency;
    std::uint16_t requested_timeout;

} connection_parameter_update_cb;

struct link_layer_with_async_parameters : unconnected_base<
    bluetoe::l2cap::signaling_channel<>,
    test::buffer_sizes,
    bluetoe::link_layer::asynchronous_connection_parameter_request< connection_parameter_update_cb_t, connection_parameter_update_cb > >
{
    link_layer_with_async_parameters()
    {
        connection_parameter_update_cb = connection_parameter_update_cb_t();
        respond_to( 37, valid_connection_request_pdu );
    }
};

using connection_parameters_configurations = std::tuple<
    link_layer_with_desired_connection_parameters,
    link_layer_with_signaling_channel,
    link_layer_with_async_parameters >;

/**
 * LL/CON/PER/BI-16-C
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Reject_Invalid_Connection_Parameter_Request_Parameters, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    // a minimum above the maximum
    link_layer.ll_control_pdu( ll_connection_param_req( { .min_interval = 20, .max_interval = 10, .latency = 3, .timeout = 496, .reference_event = 0xffff } ) );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( ll_reject_ext_ind( 0x0f, 0x1e ) );   // LL_CONNECTION_PARAM_REQ: Invalid LL Parameters
}

/**
 * LL/CON/PER/BI-08-C I
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Accepting_Connection_Parameter_Request__illegal_parameters_I, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    // a minimum below the 7.5 ms the specification allows
    link_layer.ll_control_pdu( ll_connection_param_req( { .min_interval = 4, .max_interval = 10, .latency = 3, .timeout = 496, .reference_event = 0xffff } ) );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( ll_reject_ext_ind( 0x0f, 0x1e ) );   // LL_CONNECTION_PARAM_REQ: Invalid LL Parameters
}

/**
 * LL/CON/PER/BI-08-C II
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Accepting_Connection_Parameter_Request__illegal_parameters_II, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    // a maximum above the 4 s the specification allows
    link_layer.ll_control_pdu( ll_connection_param_req( { .min_interval = 5, .max_interval = 0x0c81, .latency = 3, .timeout = 496, .reference_event = 0xffff } ) );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( ll_reject_ext_ind( 0x0f, 0x1e ) );   // LL_CONNECTION_PARAM_REQ: Invalid LL Parameters
}

/**
 * LL/CON/PER/BI-08-C III
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Accepting_Connection_Parameter_Request__illegal_parameters_III, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    // a latency above the 499 the specification allows
    link_layer.ll_control_pdu( ll_connection_param_req( { .min_interval = 5, .max_interval = 10, .latency = 500, .timeout = 496, .reference_event = 0xffff } ) );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( ll_reject_ext_ind( 0x0f, 0x1e ) );   // LL_CONNECTION_PARAM_REQ: Invalid LL Parameters
}

BOOST_FIXTURE_TEST_CASE( requesting_connect_parameters_parameters_test, link_layer_with_async_parameters )
{
    ll_control_pdu( ll_connection_param_req( with( central_request, &connection_parameter_request::latency, 5 ) ) );

    ll_empty_pdus(3);

    run( 5 );

    // neither rejected nor answered by the link layer: the host is asked
    BOOST_CHECK_EQUAL( count_transmitted( { X, X, 0x11, and_so_on } ), 0u );
    BOOST_CHECK_EQUAL( count_transmitted( { X, X, 0x10, and_so_on } ), 0u );

    BOOST_REQUIRE( connection_parameter_update_cb.remote_connection_parameter_request_received );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_interval_min, 10 );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_interval_max, 20 );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_latency, 5 );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_timeout, 2 * 20 * 4 );
}

BOOST_FIXTURE_TEST_CASE( requesting_connect_parameters_reply_test, link_layer_with_async_parameters )
{
    ll_control_pdu( ll_connection_param_req( with( central_request, &connection_parameter_request::latency, 5 ) ) );
    ll_empty_pdus(3);
    ll_function_call([this](){
        connection_parameters_request_reply( 11, 19, 3, 2 * 20 * 4 - 5 );
    });
    ll_empty_pdus(3);

    run( 5 );

    // the host's reply, without a reference event and without offsets
    check_outgoing_ll_control_pdu( ll_connection_param_rsp( {
        .min_interval = 11, .max_interval = 19, .latency = 3, .timeout = 2 * 20 * 4 - 5, .reference_event = 0xffff } ) );
}

BOOST_FIXTURE_TEST_CASE( requesting_connect_parameters_negative_reply_test, link_layer_with_async_parameters )
{
    ll_control_pdu( ll_connection_param_req( with( central_request, &connection_parameter_request::latency, 5 ) ) );
    ll_empty_pdus(3);
    ll_function_call([this](){
        connection_parameters_request_negative_reply(0x3B);
    });
    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_reject_ext_ind( 0x0f, 0x3b ) );   // LL_CONNECTION_PARAM_REQ: Unacceptable Connection Parameters
}

/**
 * LL/CON/PER/BV-30-C
 */
BOOST_FIXTURE_TEST_CASE( Accepting_Connection_Parameter_Request__preferred_anchor_points_only, link_layer_with_async_parameters )
{
    // the connection's own parameters, 30 ms and 720 ms, with one anchor point offered
    const connection_parameter_request anchor_only = {
        .min_interval = 0x18, .max_interval = 0x18, .latency = 0, .timeout = 0x48,
        .reference_event = 0x20, .offsets = { 2, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff } };

    ll_control_pdu( ll_connection_param_req( anchor_only ) );
    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( ll_connection_param_rsp( anchor_only ) );

    BOOST_REQUIRE( !connection_parameter_update_cb.remote_connection_parameter_request_received );
}

/**
 * LL/CON/PER/BV-31-C
 * LL/CON/PER/BV-32-C
 */
BOOST_FIXTURE_TEST_CASE( Accepting_Connection_Parameter_Request__Interval_Range_Not_Zero, link_layer_with_async_parameters )
{
    // an interval of 30 to 60 ms with a preferred periodicity of 10 ms
    ll_control_pdu( ll_connection_param_req( {
        .min_interval = 0x18, .max_interval = 0x30, .latency = 0, .timeout = 0x48, .preferred_periodicity = 8, .reference_event = 0xffff } ) );
    ll_empty_pdus(3);

    run( 5 );

    // in this case, the upcall to the host is required
    BOOST_REQUIRE( connection_parameter_update_cb.remote_connection_parameter_request_received );
}
