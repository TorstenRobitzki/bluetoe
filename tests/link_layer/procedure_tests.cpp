#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedure_pdus.hpp"
#include "procedures_io.hpp"

namespace bll = bluetoe::link_layer;

namespace {

    using le_ping            = bll::details::procedure_list< bll::details::le_ping_procedure >;
    using version_exchange   = bll::details::procedure_list< bll::details::version_exchange_procedure >;
    using termination        = bll::details::procedure_list< bll::details::termination_procedure >;
    using connection_update  = bll::details::procedure_list< bll::details::connection_update_indication >;
    using channel_map_update = bll::details::procedure_list< bll::details::channel_map_update_procedure >;
    using feature_exchange   = bll::details::procedure_list< bll::details::feature_exchange_procedure >;

    using phy_update         = bll::details::procedure_list< bll::details::phy_update_procedure >;

    // the two procedures with an instant, next to each other
    using connection_and_channel_map_update = bll::details::procedure_list<
        bll::details::connection_update_indication,
        bll::details::channel_map_update_procedure >;

    // the PHY update next to a procedure with an instant
    using phy_and_connection_update = bll::details::procedure_list<
        bll::details::phy_update_procedure,
        bll::details::connection_update_indication >;

    // the feature exchange with the PHY update, whose feature bit is beyond the first octet
    using feature_exchange_and_phy_update = bll::details::procedure_list<
        bll::details::feature_exchange_procedure,
        bll::details::phy_update_procedure >;

    // the feature exchange with a procedure that has a feature bit
    using feature_exchange_and_ping = bll::details::procedure_list<
        bll::details::feature_exchange_procedure,
        bll::details::le_ping_procedure >;

    // Core Vol 6, Part B, 2.4.2: error code 0x28, Instant Passed
    constexpr std::uint8_t instant_passed = 0x28;

    // Core Vol 1, Part F: error code 0x1E, Invalid LMP Parameters / Invalid LL Parameters
    constexpr std::uint8_t invalid_ll_parameters = 0x1E;

    /*
     * An update the link layer cannot follow ends the connection at once: nothing is sent, and
     * nothing is applied at the instant.
     */
    void check_update_disconnects( const connection_update_fields& fields )
    {
        link_layer_mock                         link_layer;
        link_data_mock< connection_update >     link;

        link.connection_event_counter_value = 100;

        const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( fields ) ) );

        BOOST_TEST( result == bll::details::procedure_result::disconnect( invalid_ll_parameters ) );
        BOOST_TEST( link.buffers.transmitted.empty() );

        run_connection_events( link_layer, link, fields.instant );
        BOOST_TEST( link_layer.connection_updates.empty() );
    }

    // the counterpart at the limit: the update is accepted and applied at its instant
    void check_update_is_applied( const connection_update_fields& fields )
    {
        link_layer_mock                         link_layer;
        link_data_mock< connection_update >     link;

        link.connection_event_counter_value = 100;

        const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( fields ) ) );

        BOOST_TEST( result == bll::details::procedure_result::handled() );

        run_connection_events( link_layer, link, fields.instant );
        BOOST_TEST( link_layer.connection_updates.size() == 1u );
    }
}

/*
 * LE Ping procedure
 */
BOOST_AUTO_TEST_CASE( a_ping_request_is_answered_with_a_ping_response )
{
    link_layer_mock             link_layer;
    link_data_mock< le_ping >   link;

    const auto result = le_ping::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto ping_rsp = control_pdu( {
        0x13                // LL_PING_RSP
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == ping_rsp );
}

BOOST_AUTO_TEST_CASE( a_ping_request_stalls_without_room_for_the_response )
{
    link_layer_mock             link_layer;
    link_data_mock< le_ping >   link;

    link.buffers.room_for_an_answer = false;

    const auto result = le_ping::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * Version Exchange procedure
 */
BOOST_AUTO_TEST_CASE( a_version_indication_is_answered_with_the_own_version )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

BOOST_AUTO_TEST_CASE( a_version_indication_stalls_without_room_for_the_answer )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    link.buffers.room_for_an_answer = false;

    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * A stall must not count as answered: the indication is passed again at the next connection
 * event, and then it is answered.
 */
BOOST_AUTO_TEST_CASE( a_stalled_version_indication_is_answered_when_there_is_room )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    link.buffers.room_for_an_answer = false;
    version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    link.buffers.room_for_an_answer = true;
    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

/*
 * Core Vol 6, Part B, 5.1.5: a link layer sends at most one LL_VERSION_IND per connection.
 */
BOOST_AUTO_TEST_CASE( a_second_version_indication_is_not_answered )
{
    link_layer_mock                     link_layer;
    link_data_mock< version_exchange >  link;

    version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );
    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.size() == 1u );
}

/*
 * ACL Termination procedure, started by the peer
 */
BOOST_AUTO_TEST_CASE( a_terminate_indication_disconnects_with_the_peers_error_code )
{
    link_layer_mock                 link_layer;
    link_data_mock< termination >   link;

    const auto result = termination::handle_control_pdu( link_layer, link, payload( terminate_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( 0x13 ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// there is no answer to a terminate indication, so a full transmit buffer does not stall it
BOOST_AUTO_TEST_CASE( a_terminate_indication_disconnects_without_room_for_an_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< termination >   link;

    link.buffers.room_for_an_answer = false;

    const auto result = termination::handle_control_pdu( link_layer, link, payload( terminate_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( 0x13 ) );
}

/*
 * Feature Exchange procedure, started by the central
 *
 * The peripheral answers every LL_FEATURE_REQ with an LL_FEATURE_RSP. Its FeatureSet is the
 * features of the procedures in the list: the list decides which features Bluetoe has. The list
 * itself always brings Extended Reject Indication, bit 2: Core Vol 6, Part B, 4.6.9 requires it
 * with the PHY update, 4.6.2 with the connection parameters request.
 *
 * Core Vol 6, Part B, 5.1.4.1: octet 0 of the answer is FeatureSet_USED, the features of octet 0
 * that both support; octets 1 to 7 are the peripheral's own.
 */
BOOST_AUTO_TEST_CASE( a_feature_request_is_answered_with_the_features_of_the_procedures )
{
    link_layer_mock                             link_layer;
    link_data_mock< feature_exchange_and_ping > link;

    const auto result = feature_exchange_and_ping::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 2, Extended Reject Indication; bit 4, LE Ping
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp );
}

// without a procedure that has a feature bit, the list's own feature is all there is to report
BOOST_AUTO_TEST_CASE( a_list_without_procedure_features_answers_with_extended_reject_indication )
{
    link_layer_mock                     link_layer;
    link_data_mock< feature_exchange >  link;

    const auto result = feature_exchange::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 2, Extended Reject Indication
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp );
}

/*
 * A central without any feature: octet 0 is what both support, nothing; octet 1 still carries
 * the peripheral's LE 2M PHY.
 */
BOOST_AUTO_TEST_CASE( only_the_first_octet_of_the_answer_is_shared_with_the_central )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_phy_update >   link;

    const auto request = control_pdu( {
        0x08,                                           // LL_FEATURE_REQ
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: none
    } );

    feature_exchange_and_phy_update::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x00,                                           // FeatureSet_USED: none
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00        // octets 1 to 7 of FeatureSet_P: bit 8, LE 2M PHY
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp );
}

/*
 * The features in use after the exchange: of octet 0, those both support; above it, the
 * peripheral's own. The LL_FEATURE_REQ carries the central's complete FeatureSet, so a central
 * with every feature has the LE 2M PHY as well, and the peripheral keeps using it; only the
 * answer has the octet 0 rule of 5.1.4.1.
 */
BOOST_AUTO_TEST_CASE( the_features_in_use_keep_the_peripherals_features_above_octet_0 )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_phy_update >   link;

    BOOST_REQUIRE( link.supports_feature( bll::details::link_layer_feature::le_2m_phy_support ) );

    feature_exchange_and_phy_update::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_TEST( link.supports_feature( bll::details::link_layer_feature::le_2m_phy_support ) );
    BOOST_TEST( link.supports_feature( bll::details::link_layer_feature::extended_reject_indication ) );
}

// a central with the features of octet 0 only: the LE 2M PHY is not in use anymore
BOOST_AUTO_TEST_CASE( a_feature_the_central_lacks_is_not_in_use )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_phy_update >   link;

    const auto request = control_pdu( {
        0x08,                                           // LL_FEATURE_REQ
        0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: all of octet 0, no LE 2M PHY
    } );

    feature_exchange_and_phy_update::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_TEST( !link.supports_feature( bll::details::link_layer_feature::le_2m_phy_support ) );
    BOOST_TEST( link.supports_feature( bll::details::link_layer_feature::extended_reject_indication ) );
}

BOOST_AUTO_TEST_CASE( a_feature_request_stalls_without_room_for_the_answer )
{
    link_layer_mock                             link_layer;
    link_data_mock< feature_exchange_and_ping > link;

    link.buffers.room_for_an_answer = false;

    const auto result = feature_exchange_and_ping::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// unlike the version exchange, the central may exchange the features again
BOOST_AUTO_TEST_CASE( a_second_feature_request_is_answered_too )
{
    link_layer_mock                             link_layer;
    link_data_mock< feature_exchange_and_ping > link;

    feature_exchange_and_ping::handle_control_pdu( link_layer, link, payload( feature_req ) );
    const auto result = feature_exchange_and_ping::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 2u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == link.buffers.transmitted[ 1 ] );
}

/*
 * Connection Update procedure
 *
 * Core Vol 6, Part B, 5.1.1: the peripheral applies the new parameters at the instant. An
 * instant that is not ahead of the present event has passed, and the connection is lost with
 * the error Instant Passed. Ahead means a signed 16 bit distance ( Instant - connEventCounter )
 * of 1 to 32767; LL/CON/PER/BI-04-C adds that a distance of 0 has passed.
 */
BOOST_AUTO_TEST_CASE( a_connection_update_is_applied_at_its_instant )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto update_ind = control_pdu( {
        0x00,               // LL_CONNECTION_UPDATE_IND
        0x02,               // WinSize: 2 * 1.25 ms
        0x05, 0x00,         // WinOffset: 5 * 1.25 ms
        0x28, 0x00,         // Interval: 40 * 1.25 ms
        0x03, 0x00,         // Latency: 3
        0x2C, 0x01,         // Timeout: 300 * 10 ms
        0x6A, 0x00          // Instant: 106
    } );

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( update_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    run_connection_events( link_layer, link, 105 );
    BOOST_TEST( link_layer.connection_updates.empty() );

    run_connection_events( link_layer, link, 106 );
    BOOST_REQUIRE_EQUAL( link_layer.connection_updates.size(), 1u );

    // unlike a CONNECT_IND's, the transmit window of an update has no additional delay of 1.25 ms
    const auto& timing = link_layer.connection_updates[ 0 ];
    BOOST_TEST( timing.transmit_window_size() == bll::delta_time::usec( 2500 ) );
    BOOST_TEST( timing.transmit_window_offset() == bll::delta_time::usec( 6250 ) );
    BOOST_TEST( timing.interval() == bll::delta_time::msec( 50 ) );
    BOOST_TEST( timing.latency() == 3u );
    BOOST_TEST( timing.timeout() == bll::delta_time::seconds( 3 ) );

    run_connection_events( link_layer, link, 120 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
}

// the counter reaches the value of the instant again after 65536 events; the update is done by then
BOOST_AUTO_TEST_CASE( a_connection_update_is_applied_only_once )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 106 ) ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_REQUIRE_EQUAL( link_layer.connection_updates.size(), 1u );

    // once round the counter, back to the instant
    run_connection_events( link_layer, link, 105 );
    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
}

// there is no answer to a connection update, so a full transmit buffer does not stall it
BOOST_AUTO_TEST_CASE( a_connection_update_needs_no_room_in_the_transmit_buffer )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;
    link.buffers.room_for_an_answer = false;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
}

// LL/CON/PER/BI-04-C [Rejecting Connection Change]: an instant before the present event
BOOST_AUTO_TEST_CASE( a_connection_update_with_an_instant_in_the_past_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 99 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( link_layer.connection_updates.empty() );
}

/*
 * LL/CON/PER/BI-04-C [Rejecting Connection Change]: an instant equal to the present event has
 * passed, too.
 *
 * Core Vol 6, Part B, 5.1.1 notes that a peripheral that first receives the update on the
 * instant can take that packet as the new anchor. That is what a central sees that updates one
 * event ahead and lost the first transmission; the peripheral cannot tell it from the expired
 * instant of BI-04-C, and Bluetoe follows the test.
 */
BOOST_AUTO_TEST_CASE( a_connection_update_with_an_instant_equal_to_the_present_event_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 100 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( link_layer.connection_updates.empty() );
}

/*
 * Core Vol 6, Part B, 5.5.1: an instant is in the past "where (Instant - connEventCount) mod
 * 65536 is greater than or equal to 32767". 32766 ahead is the largest distance that is pending.
 * No official test sends either; the boundary follows from the text alone.
 */
BOOST_AUTO_TEST_CASE( an_instant_32766_events_ahead_is_pending )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 100 + 32766 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 100 + 32766 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
}

BOOST_AUTO_TEST_CASE( an_instant_32767_events_ahead_has_passed )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 100 + 32767 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
    BOOST_TEST( link_layer.connection_updates.empty() );
}

BOOST_AUTO_TEST_CASE( an_instant_32768_events_ahead_has_passed )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 100 + 32768 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
}

BOOST_AUTO_TEST_CASE( a_connection_update_is_applied_at_an_instant_after_the_event_counter_wrapped )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 0xFFFE;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 0x0003 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 0x0002 );
    BOOST_TEST( link_layer.connection_updates.empty() );

    run_connection_events( link_layer, link, 0x0003 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
}

/*
 * Invalid parameters in an LL_CONNECTION_UPDATE_IND
 *
 * No test of the LL Test Suite sends them to a peripheral, and the specification defines no
 * reaction to them. Bluetoe disconnects with Invalid LL Parameters: the central switches at the
 * instant, a peripheral that ignored the update would lose the connection anyway. Each limit is
 * tested from both sides; the other fields stay valid. Units: interval, window size and window
 * offset 1.25 ms, timeout 10 ms.
 */
BOOST_AUTO_TEST_CASE( an_interval_below_7_5ms_disconnects )
{
    check_update_disconnects( { .interval = 5 } );
}

BOOST_AUTO_TEST_CASE( an_interval_of_7_5ms_is_applied )
{
    check_update_is_applied( { .interval = 6 } );
}

BOOST_AUTO_TEST_CASE( an_interval_above_4s_disconnects )
{
    check_update_disconnects( { .interval = 3201, .latency = 0, .timeout = 3200 } );
}

BOOST_AUTO_TEST_CASE( an_interval_of_4s_is_applied )
{
    check_update_is_applied( { .interval = 3200, .latency = 0, .timeout = 3200 } );
}

BOOST_AUTO_TEST_CASE( a_timeout_below_100ms_disconnects )
{
    check_update_disconnects( { .interval = 6, .latency = 0, .timeout = 9 } );
}

BOOST_AUTO_TEST_CASE( a_timeout_of_100ms_is_applied )
{
    check_update_is_applied( { .interval = 6, .latency = 0, .timeout = 10 } );
}

BOOST_AUTO_TEST_CASE( a_timeout_above_32s_disconnects )
{
    check_update_disconnects( { .timeout = 3201 } );
}

BOOST_AUTO_TEST_CASE( a_timeout_of_32s_is_applied )
{
    check_update_is_applied( { .timeout = 3200 } );
}

// the timeout has to be larger than ( 1 + latency ) * interval * 2, here 400 ms
BOOST_AUTO_TEST_CASE( a_timeout_not_larger_than_the_latency_allows_disconnects )
{
    check_update_disconnects( { .interval = 40, .latency = 3, .timeout = 40 } );
}

BOOST_AUTO_TEST_CASE( a_timeout_just_larger_than_the_latency_allows_is_applied )
{
    check_update_is_applied( { .interval = 40, .latency = 3, .timeout = 41 } );
}

BOOST_AUTO_TEST_CASE( a_latency_of_500_disconnects )
{
    check_update_disconnects( { .interval = 6, .latency = 500, .timeout = 800 } );
}

BOOST_AUTO_TEST_CASE( a_latency_of_499_is_applied )
{
    check_update_is_applied( { .interval = 6, .latency = 499, .timeout = 800 } );
}

BOOST_AUTO_TEST_CASE( a_window_size_of_0_disconnects )
{
    check_update_disconnects( { .window_size = 0 } );
}

BOOST_AUTO_TEST_CASE( a_window_size_of_1_25ms_is_applied )
{
    check_update_is_applied( { .window_size = 1 } );
}

BOOST_AUTO_TEST_CASE( a_window_size_above_10ms_disconnects )
{
    check_update_disconnects( { .window_size = 9, .interval = 11 } );
}

// the window ends at least 1.25 ms before the next interval starts
BOOST_AUTO_TEST_CASE( a_window_size_above_the_interval_less_1_25ms_disconnects )
{
    check_update_disconnects( { .window_size = 6, .interval = 6 } );
}

BOOST_AUTO_TEST_CASE( a_window_size_of_the_interval_less_1_25ms_is_applied )
{
    check_update_is_applied( { .window_size = 5, .interval = 6 } );
}

BOOST_AUTO_TEST_CASE( a_window_offset_above_the_interval_disconnects )
{
    check_update_disconnects( { .window_offset = 41, .interval = 40 } );
}

BOOST_AUTO_TEST_CASE( a_window_offset_of_the_interval_is_applied )
{
    check_update_is_applied( { .window_offset = 40, .interval = 40 } );
}

/*
 * Channel Map Update procedure
 *
 * Core Vol 6, Part B, 5.1.2: the same rules for the instant as for the connection update.
 */
BOOST_AUTO_TEST_CASE( a_channel_map_update_is_applied_at_its_instant )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;

    const auto map_ind = control_pdu( {
        0x01,                           // LL_CHANNEL_MAP_IND
        0xFF, 0x00, 0xFF, 0x00, 0x1F,   // ChM: channels 0 to 7, 16 to 23 and 32 to 36
        0x6A, 0x00                      // Instant: 106
    } );

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( map_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    run_connection_events( link_layer, link, 105 );
    BOOST_TEST( link_layer.channel_map_updates.empty() );

    run_connection_events( link_layer, link, 106 );
    BOOST_REQUIRE_EQUAL( link_layer.channel_map_updates.size(), 1u );

    const std::array< std::uint8_t, 5 > new_map({{ 0xFF, 0x00, 0xFF, 0x00, 0x1F }});
    BOOST_CHECK( link_layer.channel_map_updates[ 0 ] == new_map );

    run_connection_events( link_layer, link, 120 );
    BOOST_TEST( link_layer.channel_map_updates.size() == 1u );
}

// there is no answer to a channel map update, so a full transmit buffer does not stall it
BOOST_AUTO_TEST_CASE( a_channel_map_update_needs_no_room_in_the_transmit_buffer )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;
    link.buffers.room_for_an_answer = false;

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( channel_map_ind( 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.channel_map_updates.size() == 1u );
}

/*
 * LL/CON/PER/BI-04-C [Rejecting Connection Change] lets a peripheral either disconnect with
 * Instant Passed or ignore a channel map update with an expired instant; Bluetoe disconnects,
 * as for the connection update.
 */
BOOST_AUTO_TEST_CASE( a_channel_map_update_with_an_instant_in_the_past_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( channel_map_ind( 99 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( link_layer.channel_map_updates.empty() );
}

BOOST_AUTO_TEST_CASE( a_channel_map_update_with_an_instant_equal_to_the_present_event_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( channel_map_ind( 100 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( link_layer.channel_map_updates.empty() );
}

/*
 * A channel map has at least two used channels. No test of the LL Test Suite sends fewer to a
 * peripheral; Bluetoe disconnects with Invalid LL Parameters, as for an invalid connection
 * update.
 */
BOOST_AUTO_TEST_CASE( a_channel_map_update_with_one_channel_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;

    const auto map_ind = control_pdu( {
        0x01,                           // LL_CHANNEL_MAP_IND
        0x00, 0x00, 0x10, 0x00, 0x00,   // ChM: channel 20 only
        0x6A, 0x00                      // Instant: 106
    } );

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( map_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( invalid_ll_parameters ) );
    BOOST_TEST( link.buffers.transmitted.empty() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.channel_map_updates.empty() );
}

// the bits 37 to 39 of ChM are reserved for future use, they name no data channel
BOOST_AUTO_TEST_CASE( a_channel_map_update_with_one_channel_and_the_reserved_bits_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;

    const auto map_ind = control_pdu( {
        0x01,                           // LL_CHANNEL_MAP_IND
        0x00, 0x00, 0x10, 0x00, 0xE0,   // ChM: channel 20, and the reserved bits 37 to 39
        0x6A, 0x00                      // Instant: 106
    } );

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( map_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( invalid_ll_parameters ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( a_channel_map_update_with_two_channels_is_applied )
{
    link_layer_mock                         link_layer;
    link_data_mock< channel_map_update >    link;

    link.connection_event_counter_value = 100;

    const auto map_ind = control_pdu( {
        0x01,                           // LL_CHANNEL_MAP_IND
        0x01, 0x00, 0x00, 0x00, 0x10,   // ChM: channels 0 and 36
        0x6A, 0x00                      // Instant: 106
    } );

    const auto result = channel_map_update::handle_control_pdu( link_layer, link, payload( map_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.channel_map_updates.size() == 1u );
}

// with both procedures in the list, the instant goes to the one that set it, and only to that one
BOOST_AUTO_TEST_CASE( a_connection_update_and_a_channel_map_update_each_apply_their_own_instant )
{
    link_layer_mock                                         link_layer;
    link_data_mock< connection_and_channel_map_update >     link;

    link.connection_event_counter_value = 100;

    connection_and_channel_map_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 106 ) ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
    BOOST_TEST( link_layer.channel_map_updates.empty() );

    connection_and_channel_map_update::handle_control_pdu( link_layer, link, payload( channel_map_ind( 112 ) ) );

    run_connection_events( link_layer, link, 112 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
    BOOST_TEST( link_layer.channel_map_updates.size() == 1u );
}

/*
 * PHY Update procedure, started by the central
 *
 * Core Vol 6, Part B, 5.1.10: the central sends an LL_PHY_REQ, the peripheral answers with an
 * LL_PHY_RSP, and the central sends an LL_PHY_UPDATE_IND with the PHY of each direction and
 * the instant from which on they are used. Bluetoe has the PHY update only with a radio that
 * supports the LE 2M PHY; LE Coded is not supported.
 */
namespace {
    namespace phy = bluetoe::link_layer::phy_ll_encoding;

    const auto phy_req = control_pdu( {
        0x16,               // LL_PHY_REQ
        0x03,               // TX_PHYS: LE 1M, LE 2M
        0x03                // RX_PHYS: LE 1M, LE 2M
    } );

    // an LL_PHY_UPDATE_IND with the given PHYs and instant
    std::vector< std::uint8_t > phy_update_ind( std::uint8_t c_to_p, std::uint8_t p_to_c, std::uint16_t instant )
    {
        return control_pdu( {
            0x18, c_to_p, p_to_c,
            static_cast< std::uint8_t >( instant ), static_cast< std::uint8_t >( instant >> 8 ) } );
    }

    // Core Vol 1, Part F: error code 0x20, Unsupported LL Parameter Value
    constexpr std::uint8_t unsupported_ll_parameter_value = 0x20;

    // Core Vol 1, Part F: error code 0x2A, Different Transaction Collision
    constexpr std::uint8_t different_transaction_collision = 0x2A;
}

BOOST_AUTO_TEST_CASE( a_phy_request_is_answered_with_the_supported_phys )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto phy_rsp = control_pdu( {
        0x17,               // LL_PHY_RSP
        0x02,               // TX_PHYS: LE 2M
        0x02                // RX_PHYS: LE 2M
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == phy_rsp );
}

BOOST_AUTO_TEST_CASE( a_phy_request_stalls_without_room_for_the_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.buffers.room_for_an_answer = false;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// different PHYs for the two directions, so that swapped fields show
BOOST_AUTO_TEST_CASE( a_phy_update_is_applied_at_its_instant )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto update_ind = control_pdu( {
        0x18,               // LL_PHY_UPDATE_IND
        0x02,               // PHY_C_TO_P: LE 2M
        0x01,               // PHY_P_TO_C: LE 1M
        0x6A, 0x00          // Instant: 106
    } );

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( update_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    run_connection_events( link_layer, link, 105 );
    BOOST_TEST( link_layer.phy_updates.empty() );

    run_connection_events( link_layer, link, 106 );
    BOOST_REQUIRE_EQUAL( link_layer.phy_updates.size(), 1u );
    BOOST_CHECK(( link_layer.phy_updates[ 0 ] == link_layer_mock::phy_update{ phy::le_2m_phy, phy::le_1m_phy } ));

    run_connection_events( link_layer, link, 120 );
    BOOST_TEST( link_layer.phy_updates.size() == 1u );
}

// there is no answer to a PHY update indication, so a full transmit buffer does not stall it
BOOST_AUTO_TEST_CASE( a_phy_update_needs_no_room_in_the_transmit_buffer )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;
    link.buffers.room_for_an_answer = false;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_2m_phy, phy::le_2m_phy, 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.phy_updates.size() == 1u );
}

// a direction without a change is passed on as such
BOOST_AUTO_TEST_CASE( a_phy_update_of_one_direction_is_applied )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_unchanged_coding, phy::le_2m_phy, 106 ) ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_REQUIRE_EQUAL( link_layer.phy_updates.size(), 1u );
    BOOST_CHECK(( link_layer.phy_updates[ 0 ] == link_layer_mock::phy_update{ phy::le_unchanged_coding, phy::le_2m_phy } ));
}

/*
 * Core Vol 6, Part B, 2.4.2.24: with no change in either direction, there is no instant; the
 * Instant field is ignored. Here it would be in the past.
 */
BOOST_AUTO_TEST_CASE( a_phy_update_without_a_change_has_no_instant )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_unchanged_coding, phy::le_unchanged_coding, 99 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 120 );
    BOOST_TEST( link_layer.phy_updates.empty() );

    // no instant is pending, so a PHY update with an instant is no collision
    const auto next = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_2m_phy, phy::le_2m_phy, 126 ) ) );
    BOOST_TEST( next == bll::details::procedure_result::handled() );
}

// LL/CON/PER/BI-09-C [Responding to PHY Update Procedure – Instant In Past]
BOOST_AUTO_TEST_CASE( a_phy_update_with_an_instant_in_the_past_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_2m_phy, phy::le_2m_phy, 99 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( instant_passed ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * A PHY the peripheral cannot use, or more than one PHY for a direction: no test of the LL
 * Test Suite sends them, and the peripheral cannot follow the update. Bluetoe disconnects, as
 * for an invalid connection update.
 */
BOOST_AUTO_TEST_CASE( a_phy_update_to_the_coded_phy_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_coded_phy, phy::le_1m_phy, 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( unsupported_ll_parameter_value ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.phy_updates.empty() );
}

BOOST_AUTO_TEST_CASE( a_phy_update_with_two_phys_for_one_direction_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_1m_phy, phy::le_1m_phy | phy::le_2m_phy, 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( invalid_ll_parameters ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.phy_updates.empty() );
}

// the checks look at both fields: here the PHY from the central to the peripheral is invalid
BOOST_AUTO_TEST_CASE( a_phy_update_with_two_phys_from_the_central_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_1m_phy | phy::le_2m_phy, phy::le_1m_phy, 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( invalid_ll_parameters ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.phy_updates.empty() );
}

// and here the PHY from the peripheral to the central is one the peripheral cannot use
BOOST_AUTO_TEST_CASE( a_phy_update_to_the_coded_phy_from_the_peripheral_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< phy_update >    link;

    link.connection_event_counter_value = 100;

    const auto result = phy_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_1m_phy, phy::le_coded_phy, 106 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( unsupported_ll_parameter_value ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.phy_updates.empty() );
}

/*
 * An LL_PHY_REQ has no instant, so it is answered while another procedure's instant is
 * pending. Only an LL_PHY_UPDATE_IND before that instant would collide; after it, the update
 * is a procedure of its own.
 */
BOOST_AUTO_TEST_CASE( a_phy_request_is_answered_while_an_instant_is_pending )
{
    link_layer_mock                                 link_layer;
    link_data_mock< phy_and_connection_update >     link;

    link.connection_event_counter_value = 100;

    phy_and_connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 106 ) ) );

    const auto result = phy_and_connection_update::handle_control_pdu( link_layer, link, payload( phy_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto phy_rsp = control_pdu( {
        0x17,               // LL_PHY_RSP
        0x02,               // TX_PHYS: LE 2M
        0x02                // RX_PHYS: LE 2M
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == phy_rsp );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );

    const auto update = phy_and_connection_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_2m_phy, phy::le_2m_phy, 112 ) ) );
    BOOST_TEST( update == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 112 );
    BOOST_REQUIRE_EQUAL( link_layer.phy_updates.size(), 1u );
    BOOST_CHECK(( link_layer.phy_updates[ 0 ] == link_layer_mock::phy_update{ phy::le_2m_phy, phy::le_2m_phy } ));
}

// the instant of a PHY update collides with the pending instant of another procedure
BOOST_AUTO_TEST_CASE( a_phy_update_while_another_instant_is_pending_disconnects )
{
    link_layer_mock                                 link_layer;
    link_data_mock< phy_and_connection_update >     link;

    link.connection_event_counter_value = 100;

    phy_and_connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 106 ) ) );

    const auto result = phy_and_connection_update::handle_control_pdu( link_layer, link, payload( phy_update_ind( phy::le_2m_phy, phy::le_2m_phy, 110 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( different_transaction_collision ) );
}

// the PHY update brings the feature bit LE 2M PHY, bit 8, the first bit of the second octet
BOOST_AUTO_TEST_CASE( the_phy_update_brings_the_le_2m_phy_feature )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_phy_update >   link;

    const auto result = feature_exchange_and_phy_update::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 2, Extended Reject Indication; bit 8, LE 2M PHY
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp );
}
