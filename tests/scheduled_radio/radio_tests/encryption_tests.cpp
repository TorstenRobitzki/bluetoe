/**
 * @file encryption_tests.cpp
 *
 * The encryption of connection events, with the tester as the central: the host computes
 * the ciphertext the central sends and the ciphertext it expects the device to answer with
 * (test_tools/encryption.hpp), the device encrypts and decrypts with its radio. The CCM is
 * deterministic and the MIC covers only the LLID of the header, so a reply's bytes are
 * known in advance, and the timeline expectations state them as for any other reply. What
 * is covered: an encrypted exchange in both directions, the switches turned on one direction
 * at a time as the encryption start does it, a MIC that does not check, the packet counters
 * across repeated and empty PDUs on either side, and the exchange at 2 Mbit.
 *
 * Skipped without a tester and on a device whose radio does not encrypt.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/dut.hpp"
#include "test_tools/encryption.hpp"
#include "test_tools/observations.hpp"
#include "test_tools/records.hpp"
#include "test_tools/rig_fixture.hpp"
#include "test_tools/tester.hpp"

#include "host/central.hpp"

#include <cstdint>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    const auto if_encrypting = boost::unit_test::precondition(
        dut_supports{ &bluetoe::link_layer::radio_properties::hardware_supports_encryption } );

    using bluetoe::link_layer::phy_ll_encoding::le_2m_phy;

    constexpr auto central_to_peripheral = encryption_direction::central_to_peripheral;
    constexpr auto peripheral_to_central = encryption_direction::peripheral_to_central;

    // the long term key and the central's halves of the Core Specification's sample data
    const bluetoe::details::uint128_t long_term_key = {{
        0xbf, 0x01, 0xfb, 0x9d, 0x4e, 0xf3, 0xbc, 0x36, 0xd8, 0x74, 0xf5, 0x39, 0x41, 0x38, 0x68, 0x4c }};

    constexpr std::uint64_t skdm = 0xACBDCEDFE0F10213;
    constexpr std::uint32_t ivm  = 0xBADCAB24;

    /*
     * The connection's encryption, set up on the device before the program, as the link layer
     * does when LL_ENC_REQ arrives: the device answers its halves of the diversifier and the
     * IV, and the host derives the same session key from them.
     */
    struct encryption_fixture : connection_fixture
    {
        // not named setup(): Boost.Test calls a fixture's setup() itself, and a second call
        // would give the device a key the host does not have
        encryption_session encryption = negotiated_encryption();

        encryption_session negotiated_encryption()
        {
            const auto [ skds, ivs ] = device.call< &dut::setup_encryption >( long_term_key, skdm, ivm );

            return encryption_session( long_term_key, skdm, skds, ivm, ivs );
        }

        /**
         * @brief a data PDU of the central, encrypted with the central's `counter`
         */
        std::vector< std::uint8_t > encrypted( central& tester_side, std::uint64_t counter, std::span< const std::uint8_t > payload )
        {
            return tester_side.send( encryption.encrypt( central_to_peripheral, counter, llid::start, payload ), no_more_data, llid::start );
        }

        /**
         * @brief the payload the device sends for a queued data PDU, encrypted with the
         *        device's `counter`; reply_to() puts the header in front
         */
        std::vector< std::uint8_t > encrypted_by_device( std::uint64_t counter, std::span< const std::uint8_t > payload )
        {
            return encryption.encrypt( peripheral_to_central, counter, llid::start, payload );
        }
    };
}

BOOST_AUTO_TEST_SUITE( encryption, *if_tester *if_encrypting )

/*
 * Both directions encrypted: the central's PDU is stored decrypted, and the device's queued
 * PDU goes out encrypted with the peripheral's first counter value, its header in plain.
 */
BOOST_FIXTURE_TEST_CASE( an_encrypted_pdu_is_taken_and_answered_encrypted, encryption_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto central_data  = payload_of( 5, 0x30 );
    const auto device_data   = payload_of( 7, 0x50 );

    central tester_side;
    const auto first = encrypted( tester_side, 0, central_data );

    queue_device_pdus( { data_pdu( llid::start, device_data ) } );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            switch_encryption( true, true ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),
        received( reply_to( first, encrypted_by_device( 0, device_data ), no_more_data, llid::start ) ) } );

    check_callbacks( device_records(), {
        adv_timeout,
        connection_end_event{ .unacknowledged_data = true, .last_received_not_empty = true, .last_transmitted_not_empty = true } } );

    check_received( { as_stored( first, central_data ) } );
}

/*
 * The switches turn on one direction at a time, as the encryption start does it: after
 * LL_START_ENC_REQ the device receives encrypted and still sends in plain, after
 * LL_START_ENC_RSP it sends encrypted too. The PDU sent in plain does not use a counter value,
 * so the first ciphertext of the device uses zero, whichever event it is sent in.
 */
BOOST_FIXTURE_TEST_CASE( the_directions_switch_one_at_a_time, encryption_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto central_data  = payload_of( 5, 0x30 );
    const auto more_central  = payload_of( 6, 0x40 );
    const auto plain_data    = payload_of( 7, 0x50 );
    const auto device_data   = payload_of( 8, 0x60 );

    central tester_side;
    const auto first  = encrypted( tester_side, 0, central_data );
    const auto second = tester_side.send();
    const auto third  = encrypted( tester_side, 1, more_central );

    queue_device_pdus( { data_pdu( llid::start, plain_data ) } );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            switch_encryption( true, false ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            switch_encryption( true, true ),
            queue_pdu( data_pdu( llid::start, device_data ) ),
            next_event( data_channel ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ),
        connection_event( data_channel, interval, { third } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first, plain_data, no_more_data, llid::start ) ),
        sent( second ), received( reply_to( second, encrypted_by_device( 0, device_data ), no_more_data, llid::start ) ),
        sent( third ),  received( reply_to( third ) ) } );

    check_received( { as_stored( first, central_data ), as_stored( third, more_central ) } );
}

/*
 * A PDU whose MIC does not check is not stored, but it is acknowledged, and it does not move
 * the receive counter: the PDU after it, encrypted with the same counter value, is taken.
 */
BOOST_FIXTURE_TEST_CASE( a_pdu_with_a_wrong_mic_is_acknowledged_but_not_taken, encryption_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto first_data    = payload_of( 5, 0x30 );
    const auto second_data   = payload_of( 6, 0x40 );
    const auto third_data    = payload_of( 7, 0x50 );

    auto corrupted = encryption.encrypt( central_to_peripheral, 1, llid::start, second_data );
    corrupted.back() ^= 0x01;

    central tester_side;
    const auto first  = encrypted( tester_side, 0, first_data );
    const auto second = tester_side.send( corrupted, no_more_data, llid::start );
    const auto third  = encrypted( tester_side, 1, third_data );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            switch_encryption( true, true ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ),
        connection_event( data_channel, interval, { third } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ),
        sent( third ),  received( reply_to( third ) ) } );

    check_received( { as_stored( first, first_data ), as_stored( third, third_data ) } );
}

/*
 * The counters count new encrypted PDUs only. A PDU the central sends again, as it did not
 * get the device's reply, is the same ciphertext; the device does not store it again and
 * does not move its receive counter: the central's next new PDU, encrypted with the next
 * counter value, is taken. Empty PDUs in between move no counter either.
 */
BOOST_FIXTURE_TEST_CASE( a_repeated_pdu_of_the_central_is_not_taken_again, encryption_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto central_data  = payload_of( 5, 0x30 );
    const auto more_central  = payload_of( 6, 0x40 );

    central tester_side;
    const auto first  = encrypted( tester_side, 0, central_data );
    const auto second = tester_side.resend();
    const auto third  = tester_side.send();
    const auto fourth = encrypted( tester_side, 1, more_central );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            switch_encryption( true, true ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ),
        connection_event( data_channel, interval, { third } ),
        connection_event( data_channel, interval, { fourth } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ),
        sent( third ),  received( reply_to( third ) ),
        sent( fourth ), received( reply_to( fourth ) ) } );

    check_received( { as_stored( first, central_data ), as_stored( fourth, more_central ) } );
}

/*
 * The device's PDU the central does not acknowledge goes out again as the same ciphertext,
 * with the same counter value, behind a header that acknowledges the central's new PDU; the
 * empty PDU it answers in between uses no counter value, and the next PDU it sends, once the
 * first was acknowledged, uses the next one.
 */
BOOST_FIXTURE_TEST_CASE( the_device_repeats_an_unacknowledged_pdu_unchanged, encryption_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto device_data   = payload_of( 7, 0x50 );
    const auto more_device   = payload_of( 8, 0x60 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send_nack();
    const auto third  = tester_side.send();
    const auto fourth = tester_side.send();

    queue_device_pdus( { data_pdu( llid::start, device_data ) } );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            switch_encryption( true, true ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event(
            next_event( data_channel ) ),
        on_connection_end_event(
            queue_pdu( data_pdu( llid::start, more_device ) ),
            next_event( data_channel ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( data_channel, interval, { second } ),
        connection_event( data_channel, interval, { third } ),
        connection_event( data_channel, interval, { fourth } ) } );

    run();

    const auto ciphertext = encrypted_by_device( 0, device_data );

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first, ciphertext, no_more_data, llid::start ) ),
        sent( second ), received( reply_to( second, ciphertext, no_more_data, llid::start ) ),
        sent( third ),  received( reply_to( third ) ),
        sent( fourth ), received( reply_to( fourth, encrypted_by_device( 1, more_device ), no_more_data, llid::start ) ) } );

    check_received( {} );
}

/*
 * The same exchange at 2 Mbit, as the CCM runs at the rate of the PHY.
 */
BOOST_FIXTURE_TEST_CASE( an_encrypted_pdu_is_taken_and_answered_at_2_mbit, encryption_fixture,
    *boost::unit_test::precondition( dut_supports{ &bluetoe::link_layer::radio_properties::hardware_supports_2mbit } ) )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto central_data  = payload_of( 5, 0x30 );
    const auto device_data   = payload_of( 7, 0x50 );

    central tester_side;
    const auto first = encrypted( tester_side, 0, central_data );

    queue_device_pdus( { data_pdu( llid::start, device_data ) } );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            switch_encryption( true, true ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),
        received( reply_to( first, encrypted_by_device( 0, device_data ), no_more_data, llid::start ) ) } );

    check_received( { as_stored( first, central_data ) } );
}

BOOST_AUTO_TEST_SUITE_END()
