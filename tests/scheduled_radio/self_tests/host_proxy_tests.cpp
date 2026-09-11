#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/proxy.hpp"
#include "instrument/dispatcher.hpp"
#include "link/frame.hpp"
#include "link/ring_buffer.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    struct instrument
    {
        std::uint16_t add( std::uint16_t a, std::uint16_t b )
        {
            return static_cast< std::uint16_t >( a + b );
        }

        void store( std::uint8_t value )
        {
            stored = value;
        }

        std::uint16_t sum( const bytes< 8 >& values )
        {
            std::uint16_t result = 0;

            for ( std::uint8_t value : values.span() )
                result = static_cast< std::uint16_t >( result + value );

            return result;
        }

        std::uint8_t stored = 0;
    };

    using functions = function_list< &instrument::add, &instrument::store, &instrument::sum >;

    /*
     * Records the request and answers with whatever the test put into `response`.
     */
    struct canned_transport
    {
        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            last_request.assign( request.begin(), request.end() );

            return response;
        }

        std::vector< std::uint8_t > last_request;
        std::vector< std::uint8_t > response;
    };

    struct canned
    {
        canned_transport                        transport;
        proxy< functions, canned_transport >    remote{ transport };

        canned()
        {
            remote.expect_token( 0x11223344 );
        }

        void answer( std::initializer_list< std::uint8_t > bytes )
        {
            transport.response.assign( bytes.begin(), bytes.end() );
        }
    };

    /*
     * The real device side behind the real link pieces: the request goes through a frame
     * sender into a ring buffer, a frame receiver takes it out for the dispatcher, and
     * the response comes back the same way. No serial port, no thread.
     */
    struct loopback_transport
    {
        using buffer_t = ring_buffer< std::uint8_t, 128 >;

        instrument                                  device;
        dispatcher< functions, instrument >         dispatch{ device };

        buffer_t                                    to_device;
        buffer_t                                    to_host;
        frame_sender< buffer_t >                    host_sender{ to_device };
        frame_receiver< 64, buffer_t >              device_receiver{ to_device };
        frame_sender< buffer_t >                    device_sender{ to_host };
        frame_receiver< 64, buffer_t >              host_receiver{ to_host };

        std::uint32_t                               token = 0;

        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            if ( !host_sender.send( request ) )
                throw link_error( "request does not fit" );

            // the device's main loop, for as long as it takes
            if ( device_receiver.receive() != receive_result::frame )
                throw link_error( "the device did not receive a frame" );

            std::array< std::uint8_t, 64 > storage = {};
            buffer_sink                    out( storage );

            if ( !dispatch.dispatch( device_receiver.payload(), token, out ) )
                throw link_error( "the response does not fit" );

            if ( !device_sender.send( { storage.data(), out.size() } ) )
                throw link_error( "response does not fit" );

            if ( host_receiver.receive() != receive_result::frame )
                throw link_error( "no response frame" );

            return { host_receiver.payload().begin(), host_receiver.payload().end() };
        }
    };

    struct loopback
    {
        loopback_transport                      transport;
        proxy< functions, loopback_transport >  remote{ transport };
    };
}

BOOST_FIXTURE_TEST_CASE( a_call_becomes_opcode_and_arguments, canned )
{
    answer( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x06, 0x04 } );

    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 0x0102, 0x0304 ), 0x0406 );
    BOOST_TEST( transport.last_request == std::vector< std::uint8_t >( { 0x00, 0x02, 0x01, 0x04, 0x03 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( arguments_are_converted_to_the_parameter_types, canned )
{
    answer( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x06, 0x00 } );

    const std::uint8_t values[] = { 1, 2, 3 };

    BOOST_CHECK_EQUAL( remote.call< &instrument::sum >( values ), 6 );
    BOOST_TEST( transport.last_request == std::vector< std::uint8_t >( { 0x02, 0x03, 0x00, 0x01, 0x02, 0x03 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( a_void_function_returns_on_status_ok, canned )
{
    answer( { 0x44, 0x33, 0x22, 0x11, 0x00 } );

    BOOST_CHECK_NO_THROW( remote.call< &instrument::store >( 0x2a ) );
}

BOOST_FIXTURE_TEST_CASE( a_wrong_token_means_the_instrument_restarted, canned )
{
    answer( { 0x01, 0x00, 0x00, 0x00, 0x00 } );

    BOOST_CHECK_THROW( remote.call< &instrument::store >( 0x2a ), instrument_restarted );

    remote.expect_token( 1 );
    BOOST_CHECK_NO_THROW( remote.call< &instrument::store >( 0x2a ) );
}

BOOST_FIXTURE_TEST_CASE( a_status_other_than_ok_is_a_link_error, canned )
{
    answer( { 0x44, 0x33, 0x22, 0x11, 0x01 } );
    BOOST_CHECK_THROW( remote.call< &instrument::store >( 0x2a ), link_error );

    answer( { 0x44, 0x33, 0x22, 0x11, 0x02 } );
    BOOST_CHECK_THROW( remote.call< &instrument::store >( 0x2a ), link_error );

    answer( { 0x44, 0x33, 0x22, 0x11, 0x03 } );
    BOOST_CHECK_THROW( remote.call< &instrument::store >( 0x2a ), link_error );
}

BOOST_FIXTURE_TEST_CASE( a_response_of_the_wrong_length_is_a_link_error, canned )
{
    answer( { 0x44, 0x33, 0x22 } );
    BOOST_CHECK_THROW( remote.call< &instrument::store >( 0x2a ), link_error );

    answer( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x06 } );
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 2 ), link_error );

    answer( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x06, 0x04, 0x00 } );
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 2 ), link_error );

    answer( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x00 } );
    BOOST_CHECK_THROW( remote.call< &instrument::store >( 0x2a ), link_error );
}

/*
 * Host proxy to device dispatcher through frames and ring buffers: everything below the
 * serial port, exercised as one.
 */
BOOST_FIXTURE_TEST_CASE( a_call_travels_through_the_whole_link, loopback )
{
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 40, 2 ), 42 );

    remote.call< &instrument::store >( 0x5a );
    BOOST_CHECK_EQUAL( transport.device.stored, 0x5a );

    const std::array< std::uint8_t, 4 > values = { 10, 20, 30, 40 };
    BOOST_CHECK_EQUAL( remote.call< &instrument::sum >( values ), 100 );
}

BOOST_FIXTURE_TEST_CASE( the_token_of_the_device_reaches_the_host, loopback )
{
    transport.token = 0xdeadbeef;
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 1 ), instrument_restarted );

    remote.expect_token( 0xdeadbeef );
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 1, 1 ), 2 );
}
