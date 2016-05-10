#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <bluetoe/link_layer/l2cap_signaling_channel.hpp>

struct channel : bluetoe::l2cap::signaling_channel<>
{
    channel()
        : out_size( sizeof( buffer ) )
    {
    }

    void l2cap_input( std::initializer_list< std::uint8_t > pdu, std::initializer_list< std::uint8_t > expected )
    {
        signaling_channel::l2cap_input( pdu.begin(), pdu.size(), buffer, out_size );

        BOOST_REQUIRE_EQUAL_COLLECTIONS( expected.begin(), expected.end(), &buffer[ 0 ], &buffer[ out_size ] );
    }

    std::size_t  out_size;
    std::uint8_t buffer[ 23 ];
};

BOOST_FIXTURE_TEST_CASE( creates_no_output_by_default, channel )
{
    l2cap_output( buffer, out_size );

    BOOST_CHECK_EQUAL( out_size, 0 );
}

BOOST_FIXTURE_TEST_CASE( empty_command_to_be_rejected, channel )
{
    l2cap_input( {}, { 0x01, 0x00, 0x02, 0x00, 0x00, 0x00 } );
}

BOOST_FIXTURE_TEST_CASE( connection_parameter_update_response_without_request, channel )
{
    l2cap_input(
        {
            0x13, 0x01, 0x02, 0x00, 0x00, 0x00
        },
        {
            0x01, 0x01, 0x02, 0x00, 0x00, 0x00
        }
    );
}
