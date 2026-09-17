/**
 * @file radio_timeline_tests.cpp
 *
 * The expectations a radio test writes about the PDUs a tester program captured: how an entry
 * is rendered, and which entries of a timeline count as a difference. No instrument, no rig;
 * the reporting of a difference is check_captured()'s and is not tested here.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/timeline.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    constexpr std::array< std::uint8_t, 3 > some_bytes{ 0x02, 0x01, 0xff };
    constexpr std::array< std::uint8_t, 3 > other_bytes{ 0x02, 0x01, 0xfe };

    captured_pdu a_received( std::span< const std::uint8_t > bytes, bool crc_ok = true )
    {
        return { .direction = pdu_direction::received, .when = {}, .crc_ok = crc_ok, .rssi = 0, .data = pdu( bytes ) };
    }

    captured_pdu a_transmitted( std::span< const std::uint8_t > bytes )
    {
        return { .direction = pdu_direction::transmitted, .when = {}, .crc_ok = true, .rssi = 0, .data = pdu( bytes ) };
    }
}

BOOST_AUTO_TEST_SUITE( rendering )

BOOST_AUTO_TEST_CASE( a_received_entry_names_its_direction_its_crc_and_its_bytes )
{
    BOOST_CHECK_EQUAL( as_text( a_received( some_bytes ) ), "received crc_ok 02 01 ff" );
}

BOOST_AUTO_TEST_CASE( an_entry_the_tester_sent_is_named_sent )
{
    BOOST_CHECK_EQUAL( as_text( a_transmitted( some_bytes ) ), "sent crc_ok 02 01 ff" );
}

BOOST_AUTO_TEST_CASE( an_invalid_crc_is_named )
{
    BOOST_CHECK_EQUAL( as_text( a_received( some_bytes, false ) ), "received crc_error 02 01 ff" );
}

// what a test does not require is not rendered, so that the two lines differ in nothing else
BOOST_AUTO_TEST_CASE( a_field_the_expectation_does_not_name_is_rendered_as_a_star )
{
    BOOST_CHECK_EQUAL( as_text( a_received( some_bytes ), received_anything() ), "received crc_ok *" );
    BOOST_CHECK_EQUAL( as_text( a_received( some_bytes ), anything() ), "* * *" );
    BOOST_CHECK_EQUAL( as_text( a_transmitted( some_bytes ), sent( some_bytes ) ), "sent * 02 01 ff" );
}

BOOST_AUTO_TEST_CASE( an_expectation_renders_as_the_entry_it_requires )
{
    BOOST_CHECK_EQUAL( as_text( received( some_bytes ) ), "received crc_ok 02 01 ff" );
    BOOST_CHECK_EQUAL( as_text( received( some_bytes ).with_crc_error() ), "received crc_error 02 01 ff" );
    BOOST_CHECK_EQUAL( as_text( received_anything() ), "received crc_ok *" );
    BOOST_CHECK_EQUAL( as_text( sent( some_bytes ) ), "sent * 02 01 ff" );
    BOOST_CHECK_EQUAL( as_text( anything() ), "* * *" );
}

BOOST_AUTO_TEST_CASE( a_timeline_is_rendered_as_numbered_lines )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ), a_transmitted( other_bytes ) };

    BOOST_CHECK_EQUAL( as_text( captured ),
        "    0: received crc_ok 02 01 ff\n"
        "    1: sent crc_ok 02 01 fe\n" );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( comparing )

BOOST_AUTO_TEST_CASE( a_timeline_that_holds_what_is_expected_has_no_differences )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ), a_transmitted( other_bytes ) };

    BOOST_CHECK( differences( captured, { received( some_bytes ), sent( other_bytes ) } ).empty() );
}

BOOST_AUTO_TEST_CASE( a_difference_names_the_position_and_both_lines )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ), a_received( other_bytes ) };
    const auto found = differences( captured, { received( some_bytes ), sent( other_bytes ) } );

    BOOST_REQUIRE_EQUAL( found.size(), 1u );
    BOOST_CHECK_EQUAL( found[ 0 ], "PDU 1: received * 02 01 fe != sent * 02 01 fe" );
}

BOOST_AUTO_TEST_CASE( other_bytes_are_a_difference )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ) };
    const auto found = differences( captured, { received( other_bytes ) } );

    BOOST_REQUIRE_EQUAL( found.size(), 1u );
    BOOST_CHECK_EQUAL( found[ 0 ], "PDU 0: received crc_ok 02 01 ff != received crc_ok 02 01 fe" );
}

BOOST_AUTO_TEST_CASE( an_unexpected_crc_error_is_a_difference )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes, false ) };

    BOOST_CHECK_EQUAL( differences( captured, { received( some_bytes ) } ).size(), 1u );
    BOOST_CHECK( differences( captured, { received( some_bytes ).with_crc_error() } ).empty() );
}

BOOST_AUTO_TEST_CASE( a_crc_error_that_was_expected_and_did_not_happen_is_a_difference )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ) };

    BOOST_CHECK_EQUAL( differences( captured, { received( some_bytes ).with_crc_error() } ).size(), 1u );
}

BOOST_AUTO_TEST_CASE( an_entry_that_is_not_looked_at_is_never_a_difference )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes, false ) };

    BOOST_CHECK( differences( captured, { anything() } ).empty() );
}

BOOST_AUTO_TEST_CASE( a_received_entry_of_unknown_bytes_still_has_its_direction_and_crc_required )
{
    BOOST_CHECK( differences( { a_received( some_bytes ) }, { received_anything() } ).empty() );
    BOOST_CHECK_EQUAL( differences( { a_received( some_bytes, false ) }, { received_anything() } ).size(), 1u );
    BOOST_CHECK_EQUAL( differences( { a_transmitted( some_bytes ) }, { received_anything() } ).size(), 1u );
}

BOOST_AUTO_TEST_CASE( every_entry_that_differs_is_reported )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ), a_received( some_bytes ) };
    const auto found = differences( captured, { received( other_bytes ), sent( some_bytes ) } );

    BOOST_REQUIRE_EQUAL( found.size(), 2u );
    BOOST_CHECK( found[ 0 ].starts_with( "PDU 0: " ) );
    BOOST_CHECK( found[ 1 ].starts_with( "PDU 1: " ) );
}

// the length is check_captured()'s to require, since a test must not read past the timeline
BOOST_AUTO_TEST_CASE( only_the_entries_both_timelines_hold_are_compared )
{
    const std::vector< captured_pdu > captured = { a_received( some_bytes ), a_received( other_bytes ) };

    BOOST_CHECK( differences( captured, { received( some_bytes ) } ).empty() );
    BOOST_CHECK( differences( { captured[ 0 ] }, { received( some_bytes ), sent( other_bytes ) } ).empty() );
}

BOOST_AUTO_TEST_SUITE_END()
