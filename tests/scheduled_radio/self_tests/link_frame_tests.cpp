#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "link/crc16.hpp"
#include "link/frame.hpp"
#include "link/ring_buffer.hpp"

#include <cstdint>
#include <ostream>
#include <vector>

using namespace bluetoe::test_rig;

namespace bluetoe {
namespace test_rig {

    std::ostream& operator<<( std::ostream& out, receive_result result )
    {
        switch ( result )
        {
            case receive_result::incomplete: return out << "incomplete";
            case receive_result::frame:      return out << "frame";
            case receive_result::corrupt:    return out << "corrupt";
        }

        return out << "receive_result(" << static_cast< int >( result ) << ")";
    }
}
}

namespace {

    using buffer_t = ring_buffer< std::uint8_t, 32 >;

    struct link
    {
        buffer_t                    buffer;
        frame_sender< buffer_t >    sender{ buffer };
        frame_receiver< 16, buffer_t > receiver{ buffer };

        void arrives( std::initializer_list< std::uint8_t > bytes )
        {
            buffer.push( std::data( bytes ), bytes.size() );
        }

        std::vector< std::uint8_t > drain()
        {
            std::vector< std::uint8_t > result( buffer.available() );
            buffer.pop( result.data(), result.size() );

            return result;
        }

        std::vector< std::uint8_t > payload() const
        {
            return { receiver.payload().begin(), receiver.payload().end() };
        }
    };

    // length 3, payload aa bb cc, CRC-16/CCITT-FALSE over the five bytes
    const std::vector< std::uint8_t > frame_aabbcc = { 0x03, 0x00, 0xaa, 0xbb, 0xcc, 0xb4, 0x5f };
}

BOOST_AUTO_TEST_CASE( crc16_has_the_documented_check_value )
{
    const std::uint8_t check[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    BOOST_CHECK_EQUAL( crc16( check, sizeof( check ) ), 0x29b1 );
    BOOST_CHECK_EQUAL( crc16( check + 4, sizeof( check ) - 4, crc16( check, 4 ) ), 0x29b1 );
}

BOOST_FIXTURE_TEST_CASE( a_frame_is_length_payload_and_crc, link )
{
    const std::uint8_t payload[] = { 0xaa, 0xbb, 0xcc };

    BOOST_REQUIRE( sender.send( payload ) );
    BOOST_TEST( drain() == frame_aabbcc, boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( an_empty_payload_is_a_frame_too, link )
{
    BOOST_REQUIRE( sender.send( {} ) );
    BOOST_CHECK_EQUAL( receiver.receive(), receive_result::frame );
    BOOST_CHECK( receiver.payload().empty() );
}

BOOST_FIXTURE_TEST_CASE( a_frame_is_reported_only_when_complete, link )
{
    for ( std::size_t i = 0; i + 1 != frame_aabbcc.size(); ++i )
    {
        arrives( { frame_aabbcc[ i ] } );
        BOOST_CHECK_EQUAL( receiver.receive(), receive_result::incomplete );
    }

    arrives( { frame_aabbcc.back() } );

    BOOST_REQUIRE_EQUAL( receiver.receive(), receive_result::frame );
    BOOST_TEST( payload() == std::vector< std::uint8_t >( { 0xaa, 0xbb, 0xcc } ), boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( buffer.available(), 0u );
}

BOOST_FIXTURE_TEST_CASE( frames_arriving_together_are_taken_one_at_a_time, link )
{
    const std::uint8_t first[]  = { 1 };
    const std::uint8_t second[] = { 2, 3 };

    BOOST_REQUIRE( sender.send( first ) );
    BOOST_REQUIRE( sender.send( second ) );

    BOOST_REQUIRE_EQUAL( receiver.receive(), receive_result::frame );
    BOOST_TEST( payload() == std::vector< std::uint8_t >( { 1 } ), boost::test_tools::per_element() );

    BOOST_REQUIRE_EQUAL( receiver.receive(), receive_result::frame );
    BOOST_TEST( payload() == std::vector< std::uint8_t >( { 2, 3 } ), boost::test_tools::per_element() );

    BOOST_CHECK_EQUAL( receiver.receive(), receive_result::incomplete );
}

BOOST_FIXTURE_TEST_CASE( a_wrong_checksum_discards_everything_received, link )
{
    arrives( { 0x03, 0x00, 0xaa, 0xbb, 0xcc, 0xb5, 0x5f } );
    arrives( { 0x01, 0x00 } );

    BOOST_CHECK_EQUAL( receiver.receive(), receive_result::corrupt );
    BOOST_CHECK_EQUAL( buffer.available(), 0u );
    BOOST_CHECK( receiver.payload().empty() );
}

BOOST_FIXTURE_TEST_CASE( a_length_beyond_the_maximum_payload_is_corrupt, link )
{
    arrives( { 0x11, 0x00, 0xaa } );

    BOOST_CHECK_EQUAL( receiver.receive(), receive_result::corrupt );
    BOOST_CHECK_EQUAL( buffer.available(), 0u );
}

BOOST_FIXTURE_TEST_CASE( the_receiver_recovers_after_a_corrupt_frame, link )
{
    arrives( { 0x11, 0x00 } );
    BOOST_REQUIRE_EQUAL( receiver.receive(), receive_result::corrupt );

    buffer.push( frame_aabbcc.data(), frame_aabbcc.size() );

    BOOST_REQUIRE_EQUAL( receiver.receive(), receive_result::frame );
    BOOST_TEST( payload() == std::vector< std::uint8_t >( { 0xaa, 0xbb, 0xcc } ), boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( a_frame_that_does_not_fit_is_not_sent_at_all, link )
{
    const std::uint8_t filler[ 20 ] = {};
    const std::uint8_t payload[ 10 ] = {};

    BOOST_REQUIRE( sender.send( filler ) );
    BOOST_CHECK_EQUAL( buffer.free(), 8u );

    BOOST_CHECK( !sender.send( payload ) );
    BOOST_CHECK_EQUAL( buffer.free(), 8u );

    const std::uint8_t fits[ 4 ] = {};
    BOOST_CHECK( sender.send( fits ) );
    BOOST_CHECK_EQUAL( buffer.free(), 0u );
}

BOOST_FIXTURE_TEST_CASE( what_the_sender_sends_the_receiver_receives, link )
{
    for ( std::uint8_t round = 0; round != 50; ++round )
    {
        std::vector< std::uint8_t > payload( round % 12, round );

        BOOST_REQUIRE( sender.send( payload ) );
        BOOST_REQUIRE_EQUAL( receiver.receive(), receive_result::frame );
        BOOST_TEST( this->payload() == payload, boost::test_tools::per_element() );
    }
}
