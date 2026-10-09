#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedure_pdus.hpp"
#include "procedures_io.hpp"

#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

namespace bll = bluetoe::link_layer;

/*
 * Connection Parameters Request procedure, started by the central
 *
 * Core Vol 6, Part B, 5.1.7.2: the peripheral answers an LL_CONNECTION_PARAM_REQ with an
 * LL_CONNECTION_PARAM_RSP or rejects it with an LL_REJECT_EXT_IND. A request with a field out of
 * its valid range is rejected with Invalid LL Parameters (0x1E). The central then sends the
 * LL_CONNECTION_UPDATE_IND, which the connection update procedure handles.
 *
 * Bluetoe has two variants, chosen by a link layer option: the default accepts the central's
 * parameters as they are, desired_connection_parameters<> limits them to configured ranges.
 * Both answer at once, without asking the application, which 5.1.7.2 allows for parameters
 * within what the host has configured.
 *
 * A central that sends an LL_CONNECTION_PARAM_REQ supports Extended Reject Indication (4.6.2),
 * so every rejection is an LL_REJECT_EXT_IND, also without a feature exchange; LL/CON/PER/BI-08-C
 * expects it without one.
 */
namespace {

    using accepting_procedure       = bll::details::connection_parameters_request_procedure;

    // the interval from 10 to 20, the latency from 0 to 2, the timeout from 200 to 400
    using desired_procedure         = bll::details::desired_connection_parameters_procedure< 10, 20, 0, 2, 200, 400 >;

    using accepting     = bll::details::procedure_list< accepting_procedure >;
    using desired       = bll::details::procedure_list< desired_procedure >;

    using all_procedures = std::tuple< accepting_procedure, desired_procedure >;

    using all_variants  = std::tuple< accepting, desired >;

    // the fields of an LL_CONNECTION_PARAM_REQ or LL_CONNECTION_PARAM_RSP; by default those of the first test
    struct connection_param_fields
    {
        std::uint16_t                   interval_min                = 6;
        std::uint16_t                   interval_max                = 40;
        std::uint16_t                   latency                     = 0;
        std::uint16_t                   timeout                     = 300;
        std::uint8_t                    preferred_periodicity       = 0;
        std::uint16_t                   reference_conn_event_count  = 0;
        std::array< std::uint16_t, 6 >  offsets                     = {{ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF }};
    };

    std::vector< std::uint8_t > connection_param_pdu( std::uint8_t opcode, const connection_param_fields& fields )
    {
        const auto lsb = []( std::uint16_t value ){ return static_cast< std::uint8_t >( value ); };
        const auto msb = []( std::uint16_t value ){ return static_cast< std::uint8_t >( value >> 8 ); };

        return control_pdu( {
            opcode,
            lsb( fields.interval_min ), msb( fields.interval_min ),
            lsb( fields.interval_max ), msb( fields.interval_max ),
            lsb( fields.latency ), msb( fields.latency ),
            lsb( fields.timeout ), msb( fields.timeout ),
            fields.preferred_periodicity,
            lsb( fields.reference_conn_event_count ), msb( fields.reference_conn_event_count ),
            lsb( fields.offsets[ 0 ] ), msb( fields.offsets[ 0 ] ),
            lsb( fields.offsets[ 1 ] ), msb( fields.offsets[ 1 ] ),
            lsb( fields.offsets[ 2 ] ), msb( fields.offsets[ 2 ] ),
            lsb( fields.offsets[ 3 ] ), msb( fields.offsets[ 3 ] ),
            lsb( fields.offsets[ 4 ] ), msb( fields.offsets[ 4 ] ),
            lsb( fields.offsets[ 5 ] ), msb( fields.offsets[ 5 ] ) } );
    }

    std::vector< std::uint8_t > connection_param_req( const connection_param_fields& fields )
    {
        return connection_param_pdu( 0x0F, fields );
    }

    std::vector< std::uint8_t > connection_param_rsp( const connection_param_fields& fields )
    {
        return connection_param_pdu( 0x10, fields );
    }

    std::vector< std::uint8_t > reject_ext_ind( std::uint8_t error_code )
    {
        return control_pdu( {
            0x11,               // LL_REJECT_EXT_IND
            0x0F,               // RejectOpcode: LL_CONNECTION_PARAM_REQ
            error_code
        } );
    }

    // Core Vol 1, Part F: error code 0x1E, Invalid LMP Parameters / Invalid LL Parameters
    constexpr std::uint8_t invalid_ll_parameters = 0x1E;

    // sends the request and expects an LL_REJECT_EXT_IND with Invalid LL Parameters at once
    template < class List >
    void check_invalid_request( const connection_param_fields& fields )
    {
        link_layer_mock             link_layer;
        link_data_mock< List >      link;

        const auto result = List::handle_control_pdu( link_layer, link, payload( connection_param_req( fields ) ) );

        BOOST_TEST( result == bll::details::procedure_result::handled() );
        BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
        BOOST_TEST( link.buffers.transmitted[ 0 ] == reject_ext_ind( invalid_ll_parameters ) );
    }
}

/*
 * Every variant: checks of the request and the feature bit
 */

// LL/CON/PER/BI-08-C: an interval of 4 (5 ms) is out of the valid range
BOOST_AUTO_TEST_CASE_TEMPLATE( an_interval_out_of_range_is_rejected, List, all_variants )
{
    link_layer_mock             link_layer;
    link_data_mock< List >      link;

    const auto request = control_pdu( {
        0x0F,                           // LL_CONNECTION_PARAM_REQ
        0x04, 0x00,                     // Interval_Min: 4 * 1.25 ms
        0x04, 0x00,                     // Interval_Max: 4 * 1.25 ms
        0x00, 0x00,                     // Latency: 0
        0x2C, 0x01,                     // Timeout: 300 * 10 ms
        0x00,                           // PreferredPeriodicity: none
        0x00, 0x00,                     // ReferenceConnEventCount
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Offset0 to Offset2: not valid
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Offset3 to Offset5: not valid
    } );

    const auto result = List::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto reject = control_pdu( {
        0x11,               // LL_REJECT_EXT_IND
        0x0F,               // RejectOpcode: LL_CONNECTION_PARAM_REQ
        0x1E                // ErrorCode: Invalid LL Parameters
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == reject, boost::test_tools::per_element() );
}

// 4.5.1: connInterval starts at 7.5 ms, so 5 (6.25 ms) is out of the valid range as well
BOOST_AUTO_TEST_CASE_TEMPLATE( an_interval_of_6_25ms_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .interval_min = 5, .interval_max = 5 } );
}

// LL/CON/PER/BI-16-C
BOOST_AUTO_TEST_CASE_TEMPLATE( an_interval_max_below_the_interval_min_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .interval_min = 40, .interval_max = 20 } );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( an_interval_above_4s_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .interval_max = 3201, .timeout = 3200 } );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( a_latency_of_500_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .interval_min = 6, .interval_max = 6, .latency = 500, .timeout = 3200 } );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( a_timeout_below_100ms_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .interval_min = 6, .interval_max = 6, .timeout = 9 } );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( a_timeout_above_32s_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .timeout = 3201 } );
}

// the timeout has to be larger than 2 * Interval_Max * ( Latency + 1 ), here 2 * 50 ms * 4 = 400 ms
BOOST_AUTO_TEST_CASE_TEMPLATE( a_timeout_not_larger_than_the_latency_allows_is_rejected, List, all_variants )
{
    check_invalid_request< List >( { .interval_max = 40, .latency = 3, .timeout = 40 } );
}

// the rejection is handed to the list, which sends it once there is room; the request is handled
BOOST_AUTO_TEST_CASE_TEMPLATE( a_rejection_without_room_waits_in_the_list, List, all_variants )
{
    link_layer_mock             link_layer;
    link_data_mock< List >      link;

    link.buffers.room_for_an_answer = false;

    const auto result = List::handle_control_pdu( link_layer, link, payload( connection_param_req( { .interval_min = 4, .interval_max = 4 } ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    link.buffers.room_for_an_answer = true;
    List::connection_event( link_layer, link );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == reject_ext_ind( invalid_ll_parameters ) );
}

// the answer needs room, so the request waits
BOOST_AUTO_TEST_CASE_TEMPLATE( an_accepted_request_stalls_without_room_for_the_answer, List, all_variants )
{
    link_layer_mock             link_layer;
    link_data_mock< List >      link;

    link.buffers.room_for_an_answer = false;

    const auto result = List::handle_control_pdu( link_layer, link, payload( connection_param_req( {} ) ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// bit 1, Connection Parameters Request procedure, next to the list's own bit 2, Extended Reject Indication
BOOST_AUTO_TEST_CASE_TEMPLATE( every_variant_brings_the_connection_parameters_request_feature, Procedure, all_procedures )
{
    using features_and_list = bll::details::procedure_list< bll::details::feature_exchange_procedure, Procedure >;

    link_layer_mock                         link_layer;
    link_data_mock< features_and_list >     link;

    features_and_list::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 1, Connection Parameters Request; bit 2, Extended Reject Indication
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp, boost::test_tools::per_element() );
}

/*
 * The default: the central's parameters, as they are
 */
BOOST_AUTO_TEST_CASE( the_default_answers_with_the_centrals_parameters )
{
    link_layer_mock                 link_layer;
    link_data_mock< accepting >     link;

    const auto request = control_pdu( {
        0x0F,                           // LL_CONNECTION_PARAM_REQ
        0x06, 0x00,                     // Interval_Min: 6 * 1.25 ms
        0x28, 0x00,                     // Interval_Max: 40 * 1.25 ms
        0x00, 0x00,                     // Latency: 0
        0x2C, 0x01,                     // Timeout: 300 * 10 ms
        0x00,                           // PreferredPeriodicity: none
        0x00, 0x00,                     // ReferenceConnEventCount
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Offset0 to Offset2: not valid
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Offset3 to Offset5: not valid
    } );

    const auto result = accepting::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto response = control_pdu( {
        0x10,                           // LL_CONNECTION_PARAM_RSP
        0x06, 0x00,                     // Interval_Min: 6 * 1.25 ms
        0x28, 0x00,                     // Interval_Max: 40 * 1.25 ms
        0x00, 0x00,                     // Latency: 0
        0x2C, 0x01,                     // Timeout: 300 * 10 ms
        0x00,                           // PreferredPeriodicity: none
        0x00, 0x00,                     // ReferenceConnEventCount
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Offset0 to Offset2: not valid
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Offset3 to Offset5: not valid
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == response, boost::test_tools::per_element() );
}

// LL/CON/PER/BV-30-C: the preferred anchor points of the central are passed back as well
BOOST_AUTO_TEST_CASE( the_default_answers_with_the_centrals_anchor_points )
{
    link_layer_mock                 link_layer;
    link_data_mock< accepting >     link;

    const connection_param_fields fields{ .interval_min = 40, .interval_max = 40, .latency = 3,
        .reference_conn_event_count = 120, .offsets = {{ 1, 2, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF }} };

    accepting::handle_control_pdu( link_layer, link, payload( connection_param_req( fields ) ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == connection_param_rsp( fields ) );
}

/*
 * desired_connection_parameters<>: the central's parameters, limited to the configured ranges.
 * Where the central's range and the desired one do not overlap, the answer is the desired
 * range: alternative parameters, as 5.1.7.2 allows for parameters that are not acceptable.
 */
BOOST_AUTO_TEST_CASE( the_desired_variant_limits_the_interval_to_its_range )
{
    link_layer_mock                 link_layer;
    link_data_mock< desired >       link;

    desired::handle_control_pdu( link_layer, link, payload( connection_param_req( { .interval_min = 6, .interval_max = 40 } ) ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == connection_param_rsp( { .interval_min = 10, .interval_max = 20 } ),
        boost::test_tools::per_element() );
}

BOOST_AUTO_TEST_CASE( the_desired_variant_answers_with_its_range_if_the_ranges_do_not_overlap )
{
    link_layer_mock                 link_layer;
    link_data_mock< desired >       link;

    desired::handle_control_pdu( link_layer, link, payload( connection_param_req( { .interval_min = 30, .interval_max = 40 } ) ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == connection_param_rsp( { .interval_min = 10, .interval_max = 20 } ) );
}

/*
 * 2.4.2.16: the offsets are relative to ReferenceConnEventCount and less than Interval_Max. The
 * central's Offset0 of 30 does not fit the narrowed interval; the answer has no preference
 * about the anchor points, all offsets 0xFFFF (5.1.7.1).
 */
BOOST_AUTO_TEST_CASE( the_desired_variant_has_no_preferred_anchor_points )
{
    link_layer_mock                 link_layer;
    link_data_mock< desired >       link;

    desired::handle_control_pdu( link_layer, link, payload( connection_param_req( { .interval_min = 6, .interval_max = 40,
        .reference_conn_event_count = 120, .offsets = {{ 30, 2, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF }} } ) ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == connection_param_rsp( { .interval_min = 10, .interval_max = 20 } ) );
}

// a latency outside the desired range becomes the middle of it: ( 0 + 2 ) / 2
BOOST_AUTO_TEST_CASE( the_desired_variant_replaces_a_latency_outside_its_range )
{
    link_layer_mock                 link_layer;
    link_data_mock< desired >       link;

    desired::handle_control_pdu( link_layer, link, payload( connection_param_req( { .interval_min = 10, .interval_max = 20, .latency = 3 } ) ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == connection_param_rsp( { .interval_min = 10, .interval_max = 20, .latency = 1 } ) );
}

// a timeout outside the desired range becomes the middle of it: ( 200 + 400 ) / 2
BOOST_AUTO_TEST_CASE( the_desired_variant_replaces_a_timeout_outside_its_range )
{
    link_layer_mock                 link_layer;
    link_data_mock< desired >       link;

    desired::handle_control_pdu( link_layer, link, payload( connection_param_req( { .interval_min = 10, .interval_max = 20, .timeout = 500 } ) ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == connection_param_rsp( { .interval_min = 10, .interval_max = 20, .timeout = 300 } ) );
}
