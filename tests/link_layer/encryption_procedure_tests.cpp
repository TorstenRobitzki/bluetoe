#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedure_pdus.hpp"
#include "procedures_io.hpp"

namespace bll = bluetoe::link_layer;

namespace {

    using encryption         = bll::details::procedure_list< bll::details::encryption_procedure >;

    // the encryption next to procedures the central may start while the encryption starts
    using encryption_and_more = bll::details::procedure_list<
        bll::details::encryption_procedure,
        bll::details::version_exchange_procedure,
        bll::details::termination_procedure >;

    // the encryption next to a procedure with an instant
    using encryption_and_connection_update = bll::details::procedure_list<
        bll::details::encryption_procedure,
        bll::details::connection_update_indication >;

    // the feature exchange with the encryption, whose feature bit is bit 0
    using feature_exchange_and_encryption = bll::details::procedure_list<
        bll::details::feature_exchange_procedure,
        bll::details::encryption_procedure >;

    // a list without the encryption
    using version_exchange   = bll::details::procedure_list< bll::details::version_exchange_procedure >;
}

/*
 * Encryption Start and Encryption Pause procedures, started by the central
 *
 * Core Vol 6, Part B, 5.1.3: the central sends an LL_ENC_REQ; the peripheral answers with an
 * LL_ENC_RSP and asks the source of keys for the long term key of EDIV and Rand. With a key,
 * the peripheral sends an LL_START_ENC_REQ and from then on receives encrypted; the central
 * answers encrypted with an LL_START_ENC_RSP, and the peripheral's LL_START_ENC_RSP is its
 * first encrypted PDU. Without a key, the peripheral rejects the LL_ENC_REQ with PIN or Key
 * Missing. The LL_START_ENC_REQ or the reject is not an answer to a received PDU, so the
 * procedure sends it from the list's connection_event(), between two events.
 *
 * To refresh the key, the central pauses the encryption with an LL_PAUSE_ENC_REQ, the
 * peripheral answers encrypted with an LL_PAUSE_ENC_RSP and receives unencrypted, and the
 * central's unencrypted LL_PAUSE_ENC_RSP ends the encryption of the peripheral's transmissions.
 */
namespace {
    const auto enc_req = control_pdu( {
        0x03,                                           // LL_ENC_REQ
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, // Rand
        0x21, 0x22,                                     // EDIV
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, // SKDm
        0x41, 0x42, 0x43, 0x44                          // IVm
    } );

    const auto start_enc_req = control_pdu( {
        0x05                // LL_START_ENC_REQ
    } );

    const auto start_enc_rsp = control_pdu( {
        0x06                // LL_START_ENC_RSP
    } );

    const auto pause_enc_req = control_pdu( {
        0x0A                // LL_PAUSE_ENC_REQ
    } );

    const auto pause_enc_rsp = control_pdu( {
        0x0B                // LL_PAUSE_ENC_RSP
    } );

    // runs the encryption start, until the link is encrypted in both directions
    template < class LinkData >
    void start_encryption( link_layer_mock& link_layer, LinkData& link )
    {
        BOOST_REQUIRE( LinkData::procedures::handle_control_pdu( link_layer, link, payload( enc_req ) ) == bll::details::procedure_result::handled() );
        BOOST_REQUIRE( LinkData::procedures::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );
        BOOST_REQUIRE( LinkData::procedures::handle_control_pdu( link_layer, link, payload( start_enc_rsp ) ) == bll::details::procedure_result::handled() );
        BOOST_REQUIRE( link_layer.receive_encrypted && link_layer.transmit_encrypted );

        link.buffers.transmitted.clear();
        link_layer.encryption_changes.clear();
    }
}

BOOST_AUTO_TEST_CASE( an_encryption_request_is_answered_with_the_peripherals_session_values )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );

    // the key of EDIV and Rand is set up with the central's SKDm and IVm
    BOOST_REQUIRE_EQUAL( link_layer.key_requests.size(), 1u );
    BOOST_TEST( link_layer.key_requests[ 0 ].first == 0x2221u );
    BOOST_TEST( link_layer.key_requests[ 0 ].second == 0x1817161514131211u );

    BOOST_REQUIRE_EQUAL( link_layer.encryption_setups.size(), 1u );
    BOOST_CHECK(( link_layer.encryption_setups[ 0 ] == link_layer_mock::encryption_setup{ link_layer.key, 0x3837363534333231, 0x44434241 } ));

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto enc_rsp = control_pdu( {
        0x04,                                           // LL_ENC_RSP
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, // SKDs
        0x61, 0x62, 0x63, 0x64                          // IVs
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == enc_rsp );

    // nothing is encrypted yet
    BOOST_TEST( !link_layer.receive_encrypted );
    BOOST_TEST( !link_layer.transmit_encrypted );
}

// a stalled request is passed again; the radio is set up once, when there is room for the answer
BOOST_AUTO_TEST_CASE( an_encryption_request_stalls_without_room_for_the_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    link.buffers.room_for_an_answer = false;

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( link_layer.encryption_setups.empty() );
}

BOOST_AUTO_TEST_CASE( with_a_key_the_peripheral_starts_the_encryption )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    const auto result = encryption::connection_event( link_layer, link );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == start_enc_req );

    // the central answers encrypted; the peripheral's next PDU is still unencrypted
    BOOST_TEST( link_layer.receive_encrypted );
    BOOST_TEST( !link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes.empty() );

    // sent once
    encryption::connection_event( link_layer, link );
    BOOST_TEST( link.buffers.transmitted.size() == 1u );
}

BOOST_AUTO_TEST_CASE( the_start_of_the_encryption_waits_for_room )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    link.buffers.room_for_an_answer = false;

    BOOST_TEST( encryption::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( !link_layer.receive_encrypted );

    link.buffers.room_for_an_answer = true;

    BOOST_TEST( encryption::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == start_enc_req );
    BOOST_TEST( link_layer.receive_encrypted );
}

/*
 * Core Vol 6, Part B, 2.4.2.18: an LL_REJECT_EXT_IND only to a central that supports Extended
 * Reject Indication, otherwise an LL_REJECT_IND. Without a feature exchange, the peripheral
 * does not know, so it sends the LL_REJECT_IND that every central understands; that is what
 * LL/SEC/PER/BV-04-C requires, with a tester without LL_REJECT_EXT_IND and no exchange.
 */
BOOST_AUTO_TEST_CASE( without_a_key_the_encryption_request_is_rejected )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    link_layer.has_key = false;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    BOOST_TEST( encryption::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto reject_ind = control_pdu( {
        0x0D,               // LL_REJECT_IND
        0x06                // ErrorCode: PIN or Key Missing
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == reject_ind );
    BOOST_TEST( !link_layer.receive_encrypted );
    BOOST_TEST( !link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes.empty() );
}

// like the LL_START_ENC_REQ, the reject is sent once
BOOST_AUTO_TEST_CASE( without_a_key_the_reject_is_sent_once )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    link_layer.has_key = false;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    encryption::connection_event( link_layer, link );
    encryption::connection_event( link_layer, link );
    encryption::connection_event( link_layer, link );

    BOOST_TEST( link.buffers.transmitted.size() == 1u );
}

BOOST_AUTO_TEST_CASE( the_reject_waits_for_room )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    link_layer.has_key = false;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    link.buffers.room_for_an_answer = false;

    BOOST_TEST( encryption::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    link.buffers.room_for_an_answer = true;

    BOOST_TEST( encryption::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto reject_ind = control_pdu( {
        0x0D,               // LL_REJECT_IND
        0x06                // ErrorCode: PIN or Key Missing
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == reject_ind );
}

BOOST_AUTO_TEST_CASE( the_start_encryption_response_completes_the_encryption )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    encryption::connection_event( link_layer, link );
    link.buffers.transmitted.clear();

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( start_enc_rsp ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == start_enc_rsp );

    BOOST_TEST( link_layer.receive_encrypted );
    BOOST_TEST( link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes == std::vector< bool >{ true } );
}

// the answer is the first encrypted PDU; without room for it, the link stays as it was
BOOST_AUTO_TEST_CASE( the_start_encryption_response_stalls_without_room_for_the_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    encryption::connection_event( link_layer, link );
    link.buffers.transmitted.clear();

    link.buffers.room_for_an_answer = false;

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( start_enc_rsp ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( !link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes.empty() );
}

BOOST_AUTO_TEST_CASE( a_pause_request_is_answered_encrypted_and_ends_the_encrypted_reception )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    start_encryption( link_layer, link );

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == pause_enc_rsp );

    BOOST_TEST( !link_layer.receive_encrypted );
    BOOST_TEST( link_layer.transmit_encrypted );
}

BOOST_AUTO_TEST_CASE( a_pause_request_stalls_without_room_for_the_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    start_encryption( link_layer, link );

    link.buffers.room_for_an_answer = false;

    BOOST_TEST( encryption::handle_control_pdu( link_layer, link, payload( pause_enc_req ) ) == bll::details::procedure_result::stalled() );
    BOOST_TEST( link_layer.receive_encrypted );
    BOOST_TEST( link_layer.transmit_encrypted );
}

BOOST_AUTO_TEST_CASE( the_pause_response_of_the_central_ends_the_encryption )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    start_encryption( link_layer, link );

    encryption::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );
    link.buffers.transmitted.clear();

    // no room needed: the central's response has no answer
    link.buffers.room_for_an_answer = false;

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( pause_enc_rsp ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( !link_layer.receive_encrypted );
    BOOST_TEST( !link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes == std::vector< bool >{ false } );
}

// after a pause, the central starts the encryption with a new key
BOOST_AUTO_TEST_CASE( the_encryption_is_started_again_after_a_pause )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    start_encryption( link_layer, link );

    encryption::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );
    encryption::handle_control_pdu( link_layer, link, payload( pause_enc_rsp ) );

    start_encryption( link_layer, link );

    BOOST_TEST( link_layer.encryption_setups.size() == 2u );
}

/*
 * Between two events, connection_event() does both: it sends what the procedures have to send,
 * and applies the instant of the following event. One does not hold back the other.
 */
BOOST_AUTO_TEST_CASE( the_start_of_the_encryption_is_sent_in_the_event_of_an_instant )
{
    link_layer_mock                                         link_layer;
    link_data_mock< encryption_and_connection_update >      link;

    link.connection_event_counter_value = 100;

    const auto update_ind = control_pdu( {
        0x00,               // LL_CONNECTION_UPDATE_IND
        0x02,               // WinSize: 2 * 1.25 ms
        0x05, 0x00,         // WinOffset: 5 * 1.25 ms
        0x28, 0x00,         // Interval: 40 * 1.25 ms
        0x03, 0x00,         // Latency: 3
        0x2C, 0x01,         // Timeout: 300 * 10 ms
        0x65, 0x00          // Instant: 101
    } );

    encryption_and_connection_update::handle_control_pdu( link_layer, link, payload( update_ind ) );
    encryption_and_connection_update::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    link.connection_event_counter_value = 101;

    BOOST_TEST( encryption_and_connection_update::connection_event( link_layer, link ) == bll::details::procedure_result::handled() );

    BOOST_TEST( link_layer.connection_updates.size() == 1u );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == start_enc_req );
}

/*
 * After a feature exchange, the central's FeatureSet decides: with Extended Reject Indication an
 * LL_REJECT_EXT_IND that names the rejected LL_ENC_REQ, without it an LL_REJECT_IND.
 * LL/SEC/PER/BV-11-C exchanges the features first and accepts either.
 */
BOOST_AUTO_TEST_CASE( a_central_with_extended_reject_indication_gets_an_extended_reject )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_encryption >   link;

    link_layer.has_key = false;

    feature_exchange_and_encryption::handle_control_pdu( link_layer, link, payload( feature_req ) );
    feature_exchange_and_encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    feature_exchange_and_encryption::connection_event( link_layer, link );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto reject_ext_ind = control_pdu( {
        0x11,               // LL_REJECT_EXT_IND
        0x03,               // RejectOpcode: LL_ENC_REQ
        0x06                // ErrorCode: PIN or Key Missing
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == reject_ext_ind );
}

BOOST_AUTO_TEST_CASE( a_central_without_extended_reject_indication_gets_a_reject )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_encryption >   link;

    link_layer.has_key = false;

    const auto request = control_pdu( {
        0x08,                                           // LL_FEATURE_REQ
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 0, LE Encryption
    } );

    feature_exchange_and_encryption::handle_control_pdu( link_layer, link, payload( request ) );
    feature_exchange_and_encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    feature_exchange_and_encryption::connection_event( link_layer, link );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto reject_ind = control_pdu( {
        0x0D,               // LL_REJECT_IND
        0x06                // ErrorCode: PIN or Key Missing
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == reject_ind );
}

// the encryption brings the feature bit LE Encryption, bit 0
BOOST_AUTO_TEST_CASE( the_encryption_brings_the_le_encryption_feature )
{
    link_layer_mock                                     link_layer;
    link_data_mock< feature_exchange_and_encryption >   link;

    feature_exchange_and_encryption::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 0, LE Encryption; bit 2, Extended Reject Indication
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp );
}

/*
 * Unexpected PDUs while the encryption starts or pauses
 *
 * Core Vol 6, Part B, 5.1.3.1: from the receipt of the LL_ENC_REQ until the encryption start is
 * complete, the central only sends Empty PDUs, LL_TERMINATE_IND and the PDUs of the procedure.
 * "If, at any time during the encryption start procedure after the Peripheral has received the
 * LL_ENC_REQ PDU [...] the Link Layer of the Central or the Peripheral receives an unexpected
 * Data Physical Channel PDU from the peer Link Layer, it shall immediately exit the Connection
 * state [...] with the error code Connection Terminated Due to MIC Failure (0x3D)." 5.1.3.2 says
 * the same for the pause, from the receipt of the LL_PAUSE_ENC_REQ. LL/SEC/PER/BI-05-C sends an
 * LL_VERSION_IND, LL/SEC/PER/BI-07-C an unencrypted data PDU.
 *
 * The list sees only control PDUs. The link layer asks encryption_change_in_progress(): while
 * it is true, a received data PDU ends the connection with 0x3D, and no new data PDU is sent.
 */
namespace {
    // Core Vol 1, Part F: error code 0x3D, Connection Terminated Due to MIC Failure
    constexpr std::uint8_t mic_failure = 0x3D;
}

// LL/SEC/PER/BI-05-C
BOOST_AUTO_TEST_CASE( a_version_indication_after_the_encryption_request_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// the same, once the peripheral has sent its LL_START_ENC_REQ
BOOST_AUTO_TEST_CASE( a_version_indication_after_the_start_encryption_request_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );
    encryption_and_more::connection_event( link_layer, link );
    link.buffers.transmitted.clear();

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// an unknown opcode is unexpected too, and gets no LL_UNKNOWN_RSP
BOOST_AUTO_TEST_CASE( an_unknown_opcode_during_the_encryption_start_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

// the central may end the connection at any time
BOOST_AUTO_TEST_CASE( a_terminate_indication_during_the_encryption_start_ends_the_connection_as_usual )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( terminate_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( 0x13 ) );
}

BOOST_AUTO_TEST_CASE( after_the_encryption_start_a_version_indication_is_answered )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    start_encryption( link_layer, link );

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

// with the rejection, the encryption start is over
BOOST_AUTO_TEST_CASE( after_a_rejected_encryption_request_a_version_indication_is_answered )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    link_layer.has_key = false;

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );
    encryption_and_more::connection_event( link_layer, link );
    link.buffers.transmitted.clear();

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

BOOST_AUTO_TEST_CASE( a_version_indication_during_the_pause_disconnects )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    start_encryption( link_layer, link );

    encryption_and_more::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );
    link.buffers.transmitted.clear();

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( after_the_pause_a_version_indication_is_answered )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    start_encryption( link_layer, link );

    encryption_and_more::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );
    encryption_and_more::handle_control_pdu( link_layer, link, payload( pause_enc_rsp ) );
    link.buffers.transmitted.clear();

    const auto result = encryption_and_more::handle_control_pdu( link_layer, link, payload( version_ind ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );
    BOOST_TEST( link.buffers.transmitted[ 0 ] == own_version_ind );
}

// LL/SEC/PER/BI-07-C: the change lasts from LL_ENC_REQ and from LL_PAUSE_ENC_REQ until the procedure is complete
BOOST_AUTO_TEST_CASE( an_encryption_change_is_in_progress_while_the_encryption_starts_or_pauses )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    BOOST_TEST( !encryption_and_more::encryption_change_in_progress( link ) );

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );
    BOOST_TEST( encryption_and_more::encryption_change_in_progress( link ) );

    encryption_and_more::connection_event( link_layer, link );
    BOOST_TEST( encryption_and_more::encryption_change_in_progress( link ) );

    encryption_and_more::handle_control_pdu( link_layer, link, payload( start_enc_rsp ) );
    BOOST_TEST( !encryption_and_more::encryption_change_in_progress( link ) );

    encryption_and_more::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );
    BOOST_TEST( encryption_and_more::encryption_change_in_progress( link ) );

    encryption_and_more::handle_control_pdu( link_layer, link, payload( pause_enc_rsp ) );
    BOOST_TEST( !encryption_and_more::encryption_change_in_progress( link ) );
}

// with the rejection, the encryption start is over, and data flows unencrypted again
BOOST_AUTO_TEST_CASE( a_rejection_ends_the_encryption_change )
{
    link_layer_mock                         link_layer;
    link_data_mock< encryption_and_more >   link;

    link_layer.has_key = false;

    encryption_and_more::handle_control_pdu( link_layer, link, payload( enc_req ) );
    BOOST_TEST( encryption_and_more::encryption_change_in_progress( link ) );

    encryption_and_more::connection_event( link_layer, link );
    BOOST_TEST( !encryption_and_more::encryption_change_in_progress( link ) );
}

/*
 * The central's responses outside their procedure
 *
 * An LL_START_ENC_RSP is the central's encrypted step after the peripheral's LL_START_ENC_REQ,
 * an LL_PAUSE_ENC_RSP its step after the peripheral's LL_PAUSE_ENC_RSP. At any other time, the
 * PDU says that the central's encryption is out of step with the peripheral's: a central that
 * now switches to encrypted communication can neither read the peripheral nor be read, and a
 * plaintext link that acted on the PDU would report itself encrypted to the application while
 * it receives plaintext. The specification does not define the case; it resolves every other
 * mismatch of the handshake by leaving the connection with Connection Terminated Due to MIC
 * Failure (0x3D), and so does Bluetoe.
 */
BOOST_AUTO_TEST_CASE( a_start_encryption_response_without_an_encryption_start_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( start_enc_rsp ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( !link_layer.receive_encrypted );
    BOOST_TEST( !link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes.empty() );
}

/*
 * 5.1.3.1 covers this one: after the LL_ENC_REQ, a PDU that is not the next step is unexpected.
 * The LL_START_ENC_REQ is still waiting for room, so the LL_START_ENC_RSP comes too early.
 */
BOOST_AUTO_TEST_CASE( a_start_encryption_response_before_the_start_encryption_request_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    encryption::handle_control_pdu( link_layer, link, payload( enc_req ) );
    link.buffers.transmitted.clear();

    link.buffers.room_for_an_answer = false;
    encryption::connection_event( link_layer, link );
    link.buffers.room_for_an_answer = true;

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( start_enc_rsp ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( !link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes.empty() );
}

BOOST_AUTO_TEST_CASE( a_pause_response_without_a_pause_disconnects )
{
    link_layer_mock                 link_layer;
    link_data_mock< encryption >    link;

    start_encryption( link_layer, link );

    const auto result = encryption::handle_control_pdu( link_layer, link, payload( pause_enc_rsp ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( mic_failure ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( link_layer.receive_encrypted );
    BOOST_TEST( link_layer.transmit_encrypted );
    BOOST_TEST( link_layer.encryption_changes.empty() );
}

/*
 * A link layer without encryption
 *
 * LL/PAC/PER/BV-01-C expects an LL_UNKNOWN_RSP for every opcode the IUT does not support. Core
 * Vol 6, Part B, 5.1.3.1 asks a peripheral without encryption to reject an LL_ENC_REQ with
 * Unsupported Remote Feature (0x1A) instead. Bluetoe follows the test: without the encryption in
 * the list, LL_ENC_REQ is an unknown opcode like any other.
 */
BOOST_AUTO_TEST_CASE( without_encryption_an_encryption_request_is_unknown )
{
    link_layer_mock                         link_layer;
    link_data_mock< version_exchange >      link;

    const auto result = version_exchange::handle_control_pdu( link_layer, link, payload( enc_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x03                // UnknownType: LL_ENC_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

// without the encryption, there is never an encryption change, and data PDUs always pass
BOOST_AUTO_TEST_CASE( without_encryption_no_encryption_change_is_in_progress )
{
    link_layer_mock                         link_layer;
    link_data_mock< version_exchange >      link;

    BOOST_TEST( !version_exchange::encryption_change_in_progress( link ) );

    version_exchange::handle_control_pdu( link_layer, link, payload( enc_req ) );
    version_exchange::handle_control_pdu( link_layer, link, payload( pause_enc_req ) );

    BOOST_TEST( !version_exchange::encryption_change_in_progress( link ) );
}
