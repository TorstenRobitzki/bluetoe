/**
 * @file phy_tests.cpp
 *
 * set_phy() over the air: connection events on the 2 Mbit PHY, which only a device whose
 * radio reports it runs. The tester follows with a use_phy of its own, since nothing on air
 * says which PHY a PDU is on. Needs the tester; skipped without one, and skipped on a device
 * without 2 Mbit.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/dut.hpp"
#include "test_tools/observations.hpp"
#include "test_tools/records.hpp"
#include "test_tools/rig_fixture.hpp"
#include "test_tools/tester.hpp"

#include "host/central.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    const auto if_2mbit = boost::unit_test::precondition(
        dut_supports{ &bluetoe::link_layer::radio_properties::hardware_supports_2mbit } );

    using bluetoe::link_layer::phy_ll_encoding::le_1m_phy;
    using bluetoe::link_layer::phy_ll_encoding::le_2m_phy;
}

BOOST_AUTO_TEST_SUITE( two_mbit, *if_tester *if_2mbit )

// the same exchange as at 1 Mbit, with both sides on the other PHY
BOOST_FIXTURE_TEST_CASE( an_empty_pdu_is_answered_at_2_mbit, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto from_central = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, first_pdu_after_advertising, { from_central } ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        sent( from_central ),
        received( reply_to( from_central ) ) } );

    check_callbacks( device_records(), { adv_timeout, connection_end_event{} } );

    // the inter frame space is the same on either PHY, and only the air time before it differs
    BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 1 ], captured[ 2 ], le_2m_phy ) - 150us ), 2us );
}

// the bytes arrive as they were sent, at twice the rate
BOOST_FIXTURE_TEST_CASE( the_largest_data_pdu_of_the_central_is_received_at_2_mbit, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto payload       = payload_of( max_data_pdu_size - 2 );

    central tester_side;
    const auto data = tester_side.send( payload, no_more_data, llid::start );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, first_pdu_after_advertising, { data } ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        sent( data ),
        received( reply_to( data ) ) } );

    check_callbacks( device_records(), { adv_timeout, connection_end_event{ .last_received_not_empty = true } } );

    BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 1 ], captured[ 2 ], le_2m_phy ) - 150us ), 2us );

    check_received( { data } );
}

BOOST_FIXTURE_TEST_CASE( the_largest_data_pdu_of_the_device_is_sent_at_2_mbit, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );
    const auto payload       = payload_of( max_data_pdu_size - 2 );

    central tester_side;
    const auto from_central = tester_side.send();

    queue_device_pdus( { data_pdu( llid::start, payload ) } );

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, first_pdu_after_advertising, { from_central } ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        sent( from_central ),
        received( reply_to( from_central, payload, no_more_data, llid::start ) ) } );

    check_callbacks( device_records(), {
        adv_timeout, connection_end_event{ .unacknowledged_data = true, .last_transmitted_not_empty = true } } );

    BOOST_CHECK_LE( std::chrono::abs( inter_frame_space( captured[ 1 ], captured[ 2 ], le_2m_phy ) - 150us ), 2us );
}

/*
 * The anchor the device reports is the first bit of the first PDU it received, which it works
 * out from the end of that PDU and its air time; at 2 Mbit that air time is half. A wrong one
 * would place the next event by the same amount, which the distance of the anchors shows.
 */
BOOST_FIXTURE_TEST_CASE( connection_events_at_2_mbit_follow_each_other_at_the_interval, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            next_event( 12 ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        connection_event( 12, interval, { second } ) } );

    run();

    const auto captured = check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ) } );

    const auto records = device_records();

    check_callbacks( records, { adv_timeout, connection_end_event{}, connection_end_event{} } );

    const auto ends = callbacks_of( records, callback_kind::connection_end_event );

    BOOST_REQUIRE_EQUAL( ends.size(), 2u );
    BOOST_CHECK_LE( std::chrono::abs( time_between( captured[ 1 ], captured[ 3 ] ) - interval ), tolerance );
    BOOST_CHECK_LE( std::chrono::abs( time_between( ends[ 0 ], ends[ 1 ] ) - interval ), tolerance );
}

// nothing on air says which PHY a PDU is on, so a device listening on the other one hears nothing
BOOST_FIXTURE_TEST_CASE( a_central_on_the_other_phy_is_not_received, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_timeout() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ) } );

    check_callbacks( device_records(), { adv_timeout, connection_timeout } );
}

// what a link layer does after a PHY update: the event after the call runs on the new PHY
BOOST_FIXTURE_TEST_CASE( the_phy_can_be_changed_between_events, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first  = tester_side.send();
    const auto second = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            set_phy( le_2m_phy ),
            next_event( 12 ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        use_phy( le_2m_phy ),
        connection_event( 12, interval, { second } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),  received( reply_to( first ) ),
        sent( second ), received( reply_to( second ) ) } );

    check_callbacks( device_records(), { adv_timeout, connection_end_event{}, connection_end_event{} } );
}

/*
 * set_phy() is about connection events; advertising is on 1 Mbit whatever a connection uses,
 * so a tester that listens at 1 Mbit hears the advertising after a 2 Mbit event.
 */
BOOST_FIXTURE_TEST_CASE( advertising_stays_on_1_mbit, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event(
            set_access_address_and_crc_init( advertising_access_address, advertising_crc_init ),
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        use_access_address( advertising_access_address, advertising_crc_init ),
        use_phy( le_1m_phy ),
        receive( 37, 1, operation_timeout ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ), received( reply_to( first ) ),
        received( advertisement ) } );

    check_callbacks( device_records(), { adv_timeout, connection_end_event{}, adv_timeout } );
}

/*
 * The edges of the receive window at 2 Mbit: how long the window stays open after its end
 * depends on the PHY, since it covers the preamble, the access address and the receiver's
 * address detection of a packet that began by then. The margin of 20 µs covers the drift of
 * two stock crystals over the 50 ms both sides place the event from the advertising.
 */
BOOST_FIXTURE_TEST_CASE( a_pdu_just_before_the_receive_window_closes_is_received_at_2_mbit, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, event_start + receive_window - 20us, { first } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ),
        received( reply_to( first ) ) } );

    check_callbacks( device_records(), { adv_timeout, connection_end_event{} } );
}

BOOST_FIXTURE_TEST_CASE( a_pdu_after_the_receive_window_closed_is_not_received_at_2_mbit, connection_fixture )
{
    const auto advertisement = advertising( 6, 0x01 );

    central tester_side;
    const auto first = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            set_phy( le_2m_phy ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        on_connection_timeout() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        use_phy( le_2m_phy ),
        connection_event( data_channel, event_start + receive_window + 100us, { first } ) } );

    run();

    check_captured( {
        received( advertisement ),
        sent( first ) } );

    check_callbacks( device_records(), { adv_timeout, connection_timeout } );
}

BOOST_AUTO_TEST_SUITE_END()
