#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "buffer_io.hpp"
#include "connected.hpp"

#include <bluetoe/link_layer.hpp>
#include <bluetoe/l2cap_signaling_channel.hpp>


using namespace test;

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
};

BOOST_FIXTURE_TEST_CASE( if_central_protocol_version_is_unknown_try_ll, link_layer_with_signaling_channel )
{
    add_connection_event_respond(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x0F,                       // opcode
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
}

BOOST_FIXTURE_TEST_CASE( if_central_protocol_version_is_40_use_l2cap, link_layer_with_signaling_channel )
{
    ll_control_pdu(
        {
            0x0C,               // LL_VERSION_IND
            0x06,               // VersNr = Core Specification 4.0
            0x00, 0x02,         // CompId
            0x00, 0x00          // SubVersNr
        } );

    add_connection_event_respond(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_l2cap_pdu( {
        X,  X, 0x05, 0x00, 0x12, X, 0x08, 0x00,
        10, 0x00, 20, 0x00, 3, 0x00,
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8
    } );
}

BOOST_FIXTURE_TEST_CASE( if_centrals_features_dont_contain_ll_use_l2cap, link_layer_with_signaling_channel )
{
    ll_control_pdu(
        {
            0x08,                    // LL_FEATURE_REQ
            0x00, 0x00, 0x00, 0x00,  // no Connection Parameters Request Procedure
            0x00, 0x00, 0x00, 0x00
        } );

    add_connection_event_respond(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_l2cap_pdu( {
        X,  X, 0x05, 0x00, 0x12, X, 0x08, 0x00,
        10, 0x00, 20, 0x00, 3, 0x00,
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8
    } );
}

BOOST_FIXTURE_TEST_CASE( if_centrals_features_contain_ll_use_ll, link_layer_with_signaling_channel )
{
    ll_control_pdu(
        {
            0x08,                    // LL_FEATURE_REQ
            0x02, 0x00, 0x00, 0x00,  // Connection Parameters Request Procedure
            0x00, 0x00, 0x00, 0x00
        } );

    add_connection_event_respond(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
}

BOOST_FIXTURE_TEST_CASE( if_ll_doesn_work_fallback_to_l2cap, link_layer_with_signaling_channel )
{
    ll_function_call(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    ll_control_pdu( {
        0x07,                       // LL_UNKNOWN_RSP
        0x0F                        // LL_CONNECTION_PARAM_REQ
    } );

    ll_empty_pdus(3);

    run( 5 );

    // first tried with LL_CONNECTION_PARAM_REQ
    check_outgoing_ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    // as fallback, try l2cap
    check_outgoing_l2cap_pdu( {
        X,  X, 0x05, 0x00, 0x12, X, 0x08, 0x00,
        10, 0x00, 20, 0x00, 3, 0x00,
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8
    } );
}

BOOST_FIXTURE_TEST_CASE( if_ll_was_rejected_fallback_to_l2cap, link_layer_with_signaling_channel )
{
    ll_function_call(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    ll_control_pdu( {
        0x0D,                       // LL_REJECT_IND
        0x1A                        // Unsupported Remote/LMP Feature (0x1a)
    } );

    ll_empty_pdus(3);

    run( 5 );

    // first tried with LL_CONNECTION_PARAM_REQ
    check_outgoing_ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    // as fallback, try l2cap
    check_outgoing_l2cap_pdu( {
        X,  X, 0x05, 0x00, 0x12, X, 0x08, 0x00,
        10, 0x00, 20, 0x00, 3, 0x00,
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8
    } );
}

BOOST_FIXTURE_TEST_CASE( if_ll_was_rejected_ext_fallback_to_l2cap, link_layer_with_signaling_channel )
{
    ll_function_call(
        [&](){
            BOOST_REQUIRE( connection_parameter_update_request( 10, 20, 3, 2 * 20 * 4 ) );
        });

    ll_empty_pdus(3);

    ll_control_pdu( {
        0x11,                       // LL_REJECT_IND_EXT
        0x0F,                       // opcde
        0x1A                        // Unsupported Remote/LMP Feature (0x1a)
    } );

    ll_empty_pdus(3);

    run( 5 );

    // first tried with LL_CONNECTION_PARAM_REQ
    check_outgoing_ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    // as fallback, try l2cap
    check_outgoing_l2cap_pdu( {
        X,  X, 0x05, 0x00, 0x12, X, 0x08, 0x00,
        10, 0x00, 20, 0x00, 3, 0x00,
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8
    } );
}

BOOST_FIXTURE_TEST_CASE( copy_data_from_request, link_layer_with_signaling_channel )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
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

BOOST_FIXTURE_TEST_CASE( respond_with_desired_parameters, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        11, 0x00,                   // min interval
        19, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
}

BOOST_FIXTURE_TEST_CASE( requested_interval_outside_of_desired_values, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        20, 0x00,                   // min interval
        30, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        11, 0x00,                   // min interval
        19, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
}

BOOST_FIXTURE_TEST_CASE( requested_latency_outside_of_desired_values, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        0, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        11, 0x00,                   // min interval
        19, 0x00,                   // max interval
        11, 0x00,                   // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
}

BOOST_FIXTURE_TEST_CASE( requested_timeout_outside_of_desired_values, link_layer_with_desired_connection_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        3, 0x00,                    // latency
        (3 * 20 * 4) & 0xff, (4 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        11, 0x00,                   // min interval
        19, 0x00,                   // max interval
        3, 0x00,                    // latency
        120 & 0xff, 120 >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
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

    link_layer.ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        20, 0x00,                   // min interval
        10, 0x00,                   // max interval
        3, 0x00,                    // latency
        (3 * 20 * 4) & 0xff, (4 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0xff, 0xff,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( {
        0x11,                       // LL_REJECT_EXT_IND
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x1E                        // ErrorCode
    } );
}

/**
 * LL/CON/PER/BI-08-C I
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Accepting_Connection_Parameter_Request__illegal_parameters_I, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    link_layer.ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        4, 0x00,                    // min interval
        10, 0x00,                   // max interval
        3, 0x00,                    // latency
        (3 * 20 * 4) & 0xff, (4 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0xff, 0xff,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( {
        0x11,                       // LL_REJECT_EXT_IND
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x1E                        // ErrorCode
    } );
}

/**
 * LL/CON/PER/BI-08-C II
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Accepting_Connection_Parameter_Request__illegal_parameters_II, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    link_layer.ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        5, 0x00,                    // min interval
        0x81, 0x0C,                 // max interval
        3, 0x00,                    // latency
        (3 * 20 * 4) & 0xff, (4 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0xff, 0xff,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( {
        0x11,                       // LL_REJECT_EXT_IND
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x1E                        // ErrorCode
    } );
}

/**
 * LL/CON/PER/BI-08-C III
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( Accepting_Connection_Parameter_Request__illegal_parameters_III, fixture_t, connection_parameters_configurations )
{
    fixture_t link_layer;

    link_layer.ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        5, 0x00,                    // min interval
        10, 0x00,                   // max interval
        0xf4, 0x01,                 // latency 500
        (3 * 20 * 4) & 0xff, (4 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0xff, 0xff,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    link_layer.ll_empty_pdus(3);

    link_layer.run( 5 );

    link_layer.check_outgoing_ll_control_pdu( {
        0x11,                       // LL_REJECT_EXT_IND
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x1E                        // ErrorCode
    } );
}

BOOST_FIXTURE_TEST_CASE( requesting_connect_parameters_parameters_test, link_layer_with_async_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        5, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    ll_empty_pdus(3);

    run( 5 );

    check_connection_events( [&]( const test::connection_event& evt ) -> bool {
        using test::X;
        using test::and_so_on;

        for ( const auto& response: evt.transmitted_data )
        {
            if ( check_pdu( response, { X, X, 0x11, and_so_on } )
              || check_pdu( response, { X, X, 0x10, and_so_on } ) )
                return false;
        }

        return true;
    }, "no LL_REJECT_EXT_IND and no LL_CONNECTION_PARAM_RSP" );

    BOOST_REQUIRE( connection_parameter_update_cb.remote_connection_parameter_request_received );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_interval_min, 10 );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_interval_max, 20 );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_latency, 5 );
    BOOST_CHECK_EQUAL( connection_parameter_update_cb.requested_timeout, 2 * 20 * 4 );
}

BOOST_FIXTURE_TEST_CASE( requesting_connect_parameters_reply_test, link_layer_with_async_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        5, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
    ll_empty_pdus(3);
    ll_function_call([this](){
        connection_parameters_request_reply( 11, 19, 3, 2 * 20 * 4 - 5 );
    });
    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        11, 0x00,                   // min interval
        19, 0x00,                   // max interval
        3, 0x00,                    // latency
        (2 * 20 * 4 - 5) & 0xff, (2 * 20 * 4 - 5) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0xff, 0xff,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
}

BOOST_FIXTURE_TEST_CASE( requesting_connect_parameters_negative_reply_test, link_layer_with_async_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        10, 0x00,                   // min interval
        20, 0x00,                   // max interval
        5, 0x00,                    // latency
        (2 * 20 * 4) & 0xff, (2 * 20 * 4) >> 8, // timeout
        0x00,                       // prefered periodicity (none)
        0x00, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0x04, 0x00,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
    ll_empty_pdus(3);
    ll_function_call([this](){
        connection_parameters_request_negative_reply();
    });
    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x11,                       // LL_REJECT_EXT_IND
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x3B                        // UNACCEPTABLE CONNECTION PARAMETERS
    } );
}

/**
 * LL/CON/PER/BV-30-C
 */
BOOST_FIXTURE_TEST_CASE( Accepting_Connection_Parameter_Request__preferred_anchor_points_only, link_layer_with_async_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x18, 0x00,                 // min interval (30ms)
        0x18, 0x00,                 // max interval (30ms)
        0x00, 0x00,                 // latency latency
        0x48, 0x00,                 // connection timeout (720ms)
        0x00,                       // prefered periodicity (none)
        0x20, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
    ll_empty_pdus(3);

    run( 5 );

    check_outgoing_ll_control_pdu( {
        0x10,                       // LL_CONNECTION_PARAM_RSP
        0x18, 0x00,                 // min interval (30ms)
        0x18, 0x00,                 // max interval (30ms)
        0x00, 0x00,                 // latency latency
        0x48, 0x00,                 // connection timeout (720ms)
        0x00,                       // prefered periodicity (none)
        0x20, 0x00,                 // ReferenceConnEventCount
        0x02, 0x00,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );

    BOOST_REQUIRE( !connection_parameter_update_cb.remote_connection_parameter_request_received );
}

/**
 * LL/CON/PER/BV-31-C
 * LL/CON/PER/BV-32-C
 */
BOOST_FIXTURE_TEST_CASE( Accepting_Connection_Parameter_Request__Interval_Range_Not_Zero, link_layer_with_async_parameters )
{
    ll_control_pdu( {
        0x0F,                       // LL_CONNECTION_PARAM_REQ
        0x18, 0x00,                 // min interval (30ms)
        0x30, 0x00,                 // max interval (60ms)
        0x00, 0x00,                 // latency latency
        0x48, 0x00,                 // connection timeout (720ms)
        0x08,                       // prefered periodicity (10ms)
        0xff, 0xff,                 // ReferenceConnEventCount
        0xff, 0xff,                 // Offset0 (none)
        0xff, 0xff,                 // Offset1 (none)
        0xff, 0xff,                 // Offset2 (none)
        0xff, 0xff,                 // Offset3 (none)
        0xff, 0xff,                 // Offset4 (none)
        0xff, 0xff                  // Offset5 (none)
    } );
    ll_empty_pdus(3);

    run( 5 );

    // in this case, the upcall to the host is required
    BOOST_REQUIRE( connection_parameter_update_cb.remote_connection_parameter_request_received );
}
