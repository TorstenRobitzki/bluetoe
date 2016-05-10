#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <bluetoe/link_layer/l2cap_signaling_channel.hpp>

struct channel : bluetoe::l2cap::signaling_channel<>
{
    channel()
        : out_size( sizeof( buffer ) )
    {
    }

    void signaling_channel_input( std::initializer_list< std::uint8_t > pdu, std::initializer_list< std::uint8_t > expected )
    {
        signaling_channel::signaling_channel_input( pdu.begin(), pdu.size(), buffer, out_size );

        BOOST_REQUIRE_EQUAL_COLLECTIONS( expected.begin(), expected.end(), &buffer[ 0 ], &buffer[ out_size ] );
    }

    void signaling_channel_output( std::initializer_list< std::uint8_t > expected )
    {
        signaling_channel::signaling_channel_output( buffer, out_size );
        BOOST_REQUIRE_EQUAL_COLLECTIONS( expected.begin(), expected.end(), &buffer[ 0 ], &buffer[ out_size ] );
    }

    std::size_t  out_size;
    std::uint8_t buffer[ 23 ];
};

BOOST_FIXTURE_TEST_CASE( creates_no_output_by_default, channel )
{
    signaling_channel_output( {} );
}

BOOST_FIXTURE_TEST_CASE( empty_command_to_be_ignored, channel )
{
    signaling_channel_input( {}, {} );
}

BOOST_FIXTURE_TEST_CASE( connection_parameter_update_response_without_request, channel )
{
    signaling_channel_input(
        {
            0x13, 0x01, 0x02, 0x00, 0x00, 0x00
        },
        {
            0x01, 0x01, 0x02, 0x00, 0x00, 0x00
        }
    );
}

BOOST_FIXTURE_TEST_CASE( command_with_invalid_identifier, channel )
{
    signaling_channel_input(
        {
            0x14, 0x00, 0x0A, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        },
        {
        }
    );
}

BOOST_FIXTURE_TEST_CASE( connection_parameter_update_request_rejected, channel )
{
    signaling_channel_input(
        {
            0x12, 0x03, 0x08, 0x00,
            0x10, 0x00,
            0x20, 0x00,
            0x00, 0x00,
            0x00, 0x01,
        },
        {
            0x01, 0x03, 0x02, 0x00, 0x00, 0x00
        }
    );
}

BOOST_FIXTURE_TEST_CASE( creates_connection_parameter_update_request, channel )
{
    connection_parameter_update_request( 0x0020, 0x0100, 0x55, 0xC80 );

    signaling_channel_output( {
        0x12, 0x01, 0x08, 0x00,
        0x20, 0x00, 0x00, 0x01,
        0x55, 0x00, 0x80, 0x0c
    });
}

BOOST_FIXTURE_TEST_CASE( second_connection_parameter_update_request_will_not_be_queued, channel )
{
    BOOST_CHECK( connection_parameter_update_request( 0x0020, 0x0100, 0x55, 0xC80 ) );
    BOOST_CHECK( !connection_parameter_update_request( 0x0020, 0x0100, 0x55, 0xC80 ) );
}

struct connection_parameter_update_requested : channel
{

    connection_parameter_update_requested()
    {
        connection_parameter_update_request( 0x0020, 0x0100, 0x55, 0xC80 );

        signaling_channel_output( {
            0x12, 0x01, 0x08, 0x00,
            0x20, 0x00, 0x00, 0x01,
            0x55, 0x00, 0x80, 0x0c
        });
    }
};

BOOST_FIXTURE_TEST_CASE( second_connection_parameter_update_request_will_not_be_queued_after_output, connection_parameter_update_requested )
{
    BOOST_CHECK( !connection_parameter_update_request( 0x0020, 0x0100, 0x55, 0xC80 ) );
}

BOOST_FIXTURE_TEST_CASE( no_response_to_connection_parameter_update_response, connection_parameter_update_requested )
{
    signaling_channel_input( {
        0x13, 0x01, 0x02, 0x00,
        0x00, 0x00
    },
    {} );
}

BOOST_FIXTURE_TEST_CASE( second_connection_parameter_update_request_will_be_queued_after_response, connection_parameter_update_requested )
{
    signaling_channel_input( {
        0x13, 0x01, 0x02, 0x00,
        0x00, 0x00
    },
    {} );

    connection_parameter_update_request( 0x0020, 0x0100, 0x55, 0xC80 );

    signaling_channel_output( {
        0x12, 0x02, 0x08, 0x00,
        0x20, 0x00, 0x00, 0x01,
        0x55, 0x00, 0x80, 0x0c
    });
}