/**
 * @file host_transport_tests.cpp
 *
 * The stream transport against a device on the other end of a pair of loopback sockets:
 * a thread on the device's socket runs a real dispatcher behind real frames, with a
 * switch for what a misbehaving device would do. The serial transport adds only the
 * opening of a port, which is checked with a device that does not exist.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/errors.hpp"
#include "host/proxy.hpp"
#include "host/serial_transport.hpp"
#include "host/stream_transport.hpp"
#include "instrument/dispatcher.hpp"
#include "link/frame.hpp"
#include "link/ring_buffer.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    using boost::asio::ip::tcp;

    struct instrument
    {
        std::uint16_t add( std::uint16_t a, std::uint16_t b )
        {
            return static_cast< std::uint16_t >( a + b );
        }
    };

    using functions = function_list< &instrument::add >;

    const auto timeout = 100ms;

    /*
     * Two connected sockets on the loopback interface, one for each end.
     */
    struct socket_pair
    {
        socket_pair()
            : acceptor( io, tcp::endpoint( boost::asio::ip::make_address( "127.0.0.1" ), 0 ) )
            , host( io )
            , device( io )
        {
            host.connect( acceptor.local_endpoint() );
            acceptor.accept( device );
        }

        boost::asio::io_context io;
        tcp::acceptor           acceptor;
        tcp::socket             host;
        tcp::socket             device;
    };

    enum class behaviour
    {
        answer,
        swallow,
        corrupt,
        answer_late
    };

    /*
     * The device at its end of the pair. Boost.Test macros are not used in its thread;
     * the thread ends when the socket is shut down.
     */
    class fake_device
    {
    public:
        explicit fake_device( tcp::socket& socket )
            : socket_( socket )
            , thread_( [ this ]() { run(); } )
        {
        }

        ~fake_device()
        {
            boost::system::error_code ignored;
            socket_.shutdown( tcp::socket::shutdown_both, ignored );
            thread_.join();
        }

        std::atomic< behaviour >    mode{ behaviour::answer };
        std::atomic< int >          answered{ 0 };

    private:
        void run()
        {
            using buffer_t = ring_buffer< std::uint8_t, 1024 >;

            instrument                              device;
            buffer_t                                in;
            buffer_t                                out;
            frame_receiver< 256, buffer_t >         receiver( in );
            frame_sender< buffer_t >                sender( out );
            dispatcher< functions, instrument >     dispatch( device );

            for ( ;; )
            {
                std::uint8_t                chunk[ 256 ];
                boost::system::error_code   error;
                const std::size_t           count = socket_.read_some( boost::asio::buffer( chunk ), error );

                if ( error )
                    return;

                in.push( chunk, count );

                if ( receiver.receive() != receive_result::frame )
                    continue;

                if ( mode == behaviour::swallow )
                    continue;

                if ( mode == behaviour::answer_late )
                    std::this_thread::sleep_for( 3 * timeout );

                std::array< std::uint8_t, 64 > storage = {};
                buffer_sink                    response( storage );

                if ( !dispatch.dispatch( receiver.payload(), 0, response ) )
                    continue;

                sender.send( { storage.data(), response.size() } );

                std::vector< std::uint8_t > bytes( out.available() );
                out.pop( bytes.data(), bytes.size() );

                if ( mode == behaviour::corrupt )
                    bytes.back() ^= 0xff;

                boost::asio::write( socket_, boost::asio::buffer( bytes ), error );
                ++answered;
            }
        }

        tcp::socket&    socket_;
        std::thread     thread_;
    };

    struct fixture
    {
        socket_pair                                     sockets;
        fake_device                                     device{ sockets.device };
        stream_transport< tcp::socket >                 transport{ sockets.io, sockets.host, "the device", timeout };
        proxy< functions, stream_transport< tcp::socket > > remote{ transport };
    };
}

BOOST_FIXTURE_TEST_CASE( a_call_travels_through_the_stream, fixture )
{
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 40, 2 ), 42 );
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 1000, 234 ), 1234 );
}

BOOST_FIXTURE_TEST_CASE( no_response_within_the_timeout_is_a_link_error, fixture )
{
    device.mode = behaviour::swallow;

    const auto start = std::chrono::steady_clock::now();
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 2 ), link_error );
    BOOST_CHECK( std::chrono::steady_clock::now() - start >= timeout );
}

BOOST_FIXTURE_TEST_CASE( a_corrupt_response_is_a_link_error_and_the_next_call_works, fixture )
{
    device.mode = behaviour::corrupt;
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 2 ), link_error );

    device.mode = behaviour::answer;
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 1, 2 ), 3 );
}

/*
 * The device answers the first request after the host gave up on it. That answer must not
 * be taken for the answer to the second request.
 */
BOOST_FIXTURE_TEST_CASE( a_late_response_is_not_taken_for_the_next_one, fixture )
{
    device.mode = behaviour::answer_late;
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 2 ), link_error );

    for ( int waited = 0; device.answered == 0 && waited < 100; ++waited )
        std::this_thread::sleep_for( 10ms );

    BOOST_REQUIRE_EQUAL( device.answered, 1 );

    device.mode = behaviour::answer;
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 10, 20 ), 30 );
}

BOOST_AUTO_TEST_CASE( a_serial_device_that_does_not_exist_cannot_be_opened )
{
    BOOST_CHECK_THROW( serial_transport( "/dev/bluetoe-does-not-exist", timeout ), rig_error );
}
