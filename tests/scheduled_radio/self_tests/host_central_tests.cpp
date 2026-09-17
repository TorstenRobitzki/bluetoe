#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/central.hpp"

#include <cstdint>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    const std::uint8_t payload[] = { 1, 2, 3 };

    // a reply from the device with `sn` and `nesn`, as the tester captures it
    captured_pdu reply( bool sn, bool nesn )
    {
        const std::uint8_t header = 0x01 | ( nesn ? 0x04 : 0 ) | ( sn ? 0x08 : 0 );
        const std::uint8_t bytes[] = { header, 0 };

        return captured_pdu{ .direction = pdu_direction::received, .when = {}, .crc_ok = true, .data = pdu( bytes ) };
    }
}

BOOST_AUTO_TEST_CASE( the_first_pdu_starts_both_sequence_numbers_at_zero )
{
    central c;
    const auto first = c.send();

    BOOST_REQUIRE_EQUAL( first.size(), 2u );
    BOOST_CHECK_EQUAL( first[ 0 ], 0x01 );
    BOOST_CHECK_EQUAL( first[ 1 ], 0 );
}

BOOST_AUTO_TEST_CASE( a_pdu_carries_its_llid_payload_and_more_data )
{
    central c;
    const auto sent = c.send( payload, true, llid::start );

    const std::vector< std::uint8_t > expected = { 0x02 | 0x10, 3, 1, 2, 3 };
    BOOST_CHECK( sent == expected );
    BOOST_CHECK( more_data( sent ) );
}

// the normal flow advances both bits with every PDU
BOOST_AUTO_TEST_CASE( sending_advances_sn_and_nesn )
{
    central c;
    c.send();
    const auto second = c.send();
    const auto third  = c.send();

    BOOST_CHECK( sequence_number( second ) );
    BOOST_CHECK( next_expected_sequence_number( second ) );
    BOOST_CHECK( !sequence_number( third ) );
    BOOST_CHECK( !next_expected_sequence_number( third ) );
}

// not acknowledged: the same SN and bytes, the device's reply acknowledged
BOOST_AUTO_TEST_CASE( a_resend_keeps_sn_and_the_payload_and_advances_nesn )
{
    central c;
    const auto first = c.send( payload );
    const auto again = c.resend();

    BOOST_CHECK_EQUAL( sequence_number( again ), sequence_number( first ) );
    BOOST_CHECK( next_expected_sequence_number( again ) );
    BOOST_CHECK( std::vector< std::uint8_t >( again.begin() + 1, again.end() ) == std::vector< std::uint8_t >( first.begin() + 1, first.end() ) );

    const auto next = c.send();
    BOOST_CHECK( sequence_number( next ) );
    BOOST_CHECK( !next_expected_sequence_number( next ) );
}

// the device's reply not accepted: the SN advances, NESN stays
BOOST_AUTO_TEST_CASE( a_nack_advances_sn_and_keeps_nesn )
{
    central c;
    c.send();
    const auto nack = c.send_nack();

    BOOST_CHECK( sequence_number( nack ) );
    BOOST_CHECK( !next_expected_sequence_number( nack ) );
}

BOOST_AUTO_TEST_CASE( a_reply_acknowledges_when_its_nesn_follows_the_sn_sent )
{
    central c;
    const auto first = c.send();

    BOOST_CHECK( acknowledges( reply( false, true ), first ) );
    BOOST_CHECK( !acknowledges( reply( false, false ), first ) );
}

BOOST_AUTO_TEST_CASE( a_reply_is_new_when_its_sn_is_the_one_expected )
{
    central c;
    c.send();
    const auto second = c.send();

    BOOST_CHECK( is_new( reply( true, false ), second ) );
    BOOST_CHECK( !is_new( reply( false, false ), second ) );
}
