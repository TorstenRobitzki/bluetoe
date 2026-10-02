#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedures_io.hpp"

namespace bll = bluetoe::link_layer;

namespace {

    using le_ping            = bll::details::procedure_list< bll::details::le_ping_procedure >;
    using version_exchange   = bll::details::procedure_list< bll::details::version_exchange_procedure >;
    using termination        = bll::details::procedure_list< bll::details::termination_procedure >;
    using connection_update  = bll::details::procedure_list< bll::details::connection_update_indication >;
    using channel_map_update = bll::details::procedure_list< bll::details::channel_map_update_procedure >;

    const auto ping_req = control_pdu( {
        0x12                // LL_PING_REQ
    } );

    const auto version_ind = control_pdu( {
        0x0C,               // LL_VERSION_IND
        0x0D,               // VersNr: Core 5.4
        0x59, 0x00,         // CompId: Nordic Semiconductor
        0x34, 0x12          // SubVersNr
    } );

    const auto own_version_ind = control_pdu( {
        0x0C,               // LL_VERSION_IND
        0x09,               // VersNr: Core 5.0
        0x69, 0x02,         // CompId: 0x0269
        0x00, 0x00          // SubVersNr
    } );

    const auto terminate_ind = control_pdu( {
        0x02,               // LL_TERMINATE_IND
        0x13                // ErrorCode: Remote User Terminated Connection
    } );

    // the fields of an LL_CONNECTION_UPDATE_IND; by default those of the first connection update test
    struct connection_update_fields
    {
        std::uint8_t    window_size     = 2;
        std::uint16_t   window_offset   = 5;
        std::uint16_t   interval        = 40;
        std::uint16_t   latency         = 3;
        std::uint16_t   timeout         = 300;
        std::uint16_t   instant         = 106;
    };

    std::vector< std::uint8_t > connection_update_ind( const connection_update_fields& fields )
    {
        const auto lsb = []( std::uint16_t value ){ return static_cast< std::uint8_t >( value ); };
        const auto msb = []( std::uint16_t value ){ return static_cast< std::uint8_t >( value >> 8 ); };

        return control_pdu( {
            0x00, fields.window_size,
            lsb( fields.window_offset ), msb( fields.window_offset ),
            lsb( fields.interval ), msb( fields.interval ),
            lsb( fields.latency ), msb( fields.latency ),
            lsb( fields.timeout ), msb( fields.timeout ),
            lsb( fields.instant ), msb( fields.instant ) } );
    }

    // the connection update of the first connection update test, with another instant
    std::vector< std::uint8_t > connection_update_ind( std::uint16_t instant )
    {
        return connection_update_ind( connection_update_fields{ .instant = instant } );
    }

    // the channel map update of the first channel map update test, with another instant
    std::vector< std::uint8_t > channel_map_ind( std::uint16_t instant )
    {
        return control_pdu( {
            0x01, 0xFF, 0x00, 0xFF, 0x00, 0x1F,
            static_cast< std::uint8_t >( instant ), static_cast< std::uint8_t >( instant >> 8 ) } );
    }

    /*
     * The link layer calls connection_event() before it sets up an event, with the counter of
     * that event. This runs the events after the current one, up to and including last.
     */
    template < class Procedures >
    void run_connection_events( link_layer_mock& link_layer, link_data_mock< Procedures >& link, std::uint16_t last )
    {
        while ( link.connection_event_counter_value != last )
        {
            ++link.connection_event_counter_value;

            const auto result = Procedures::connection_event( link_layer, link );
            BOOST_TEST( result == bll::details::procedure_result::handled() );
        }
    }

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

// the largest distance ahead; no official test sends it, it follows from the signed distance
BOOST_AUTO_TEST_CASE( an_instant_32767_events_ahead_is_pending )
{
    link_layer_mock                         link_layer;
    link_data_mock< connection_update >     link;

    link.connection_event_counter_value = 100;

    const auto result = connection_update::handle_control_pdu( link_layer, link, payload( connection_update_ind( 100 + 32767 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 100 + 32767 );
    BOOST_TEST( link_layer.connection_updates.size() == 1u );
}

// 32768 ahead is a signed distance of -32768, so it has passed; this, too, follows from the signed distance only
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
