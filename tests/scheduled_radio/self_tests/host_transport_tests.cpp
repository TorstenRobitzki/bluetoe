/**
 * @file host_transport_tests.cpp
 *
 * The stream transport against a device the test drives: a stream that delivers what the test
 * says and keeps what the transport wrote, and behind it a real dispatcher behind real frames,
 * with a switch for what a misbehaving device would do. Everything runs on the test's thread,
 * so no assertion here depends on when anything is scheduled. The serial transport adds only
 * the opening of a port, which is checked with a device that does not exist.
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
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

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
     * A stream the test drives in place of a port: nothing arrives unless the test says so,
     * what the transport writes is kept, and a read stays pending until the test answers it
     * or the transport cancels it, as a port's read does. Everything happens on the test's
     * thread, so what a test asserts does not depend on when anything is scheduled.
     */
    class driven_stream
    {
    public:
        using executor_type = boost::asio::io_context::executor_type;

        /**
         * @brief a device behind the stream: what it answers a frame the transport wrote with,
         *        or nothing
         */
        using answering = std::function< std::vector< std::uint8_t >( std::span< const std::uint8_t > ) >;

        explicit driven_stream( boost::asio::io_context& io )
            : io_( io )
        {
        }

        executor_type get_executor()
        {
            return io_.get_executor();
        }

        template < typename ConstBufferSequence >
        std::size_t write_some( const ConstBufferSequence& buffers, boost::system::error_code& error )
        {
            error = {};

            const std::size_t size = boost::asio::buffer_size( buffers );
            const std::size_t at   = written_.size();

            written_.resize( at + size );
            boost::asio::buffer_copy( boost::asio::buffer( written_.data() + at, size ), buffers );

            // a device answers what it was asked, so the answer arrives with the request and
            // not before it, where the transport would drop it as stale
            if ( answering_ )
            {
                const std::vector< std::uint8_t > answer = answering_( { written_.data() + at, size } );

                if ( !answer.empty() )
                    arrives( answer );
            }

            return size;
        }

        template < typename MutableBufferSequence, typename Handler >
        void async_read_some( const MutableBufferSequence& buffers, Handler handler )
        {
            pending_ = [ this, buffers, handler ]( const boost::system::error_code& error ) {
                const std::size_t size = error ? 0 : std::min( boost::asio::buffer_size( buffers ), arrived_.size() );

                boost::asio::buffer_copy( buffers, boost::asio::buffer( arrived_.data(), size ) );
                arrived_.erase( arrived_.begin(), arrived_.begin() + size );

                handler( error, size );
            };

            if ( !arrived_.empty() )
                finish( {} );
        }

        void cancel()
        {
            if ( pending_ )
                finish( boost::asio::error::operation_aborted );
        }

        /**
         * @brief the device behind the stream: what it answers what the transport writes with
         */
        void answers_with( answering device )
        {
            answering_ = std::move( device );
        }

        /**
         * @brief what the device would have sent, for the reads that follow
         */
        void arrives( std::span< const std::uint8_t > bytes )
        {
            arrived_.insert( arrived_.end(), bytes.begin(), bytes.end() );

            if ( pending_ )
                finish( {} );
        }

        /**
         * @brief everything the transport has written so far
         */
        const std::vector< std::uint8_t >& written() const
        {
            return written_;
        }

    private:
        void finish( boost::system::error_code error )
        {
            boost::asio::post( io_, [ error, done = std::move( pending_ ) ]() {
                done( error );
            } );

            pending_ = {};
        }

        boost::asio::io_context&                                    io_;
        answering                                                   answering_;
        std::vector< std::uint8_t >                                 arrived_;
        std::vector< std::uint8_t >                                 written_;
        std::function< void( const boost::system::error_code& ) >   pending_;
    };

    enum class behaviour
    {
        answer,
        swallow,
        corrupt
    };

    /*
     * The device at the far end of a driven stream, on the test's thread: it takes what the
     * transport wrote, and answers a complete frame with what the dispatcher makes of it.
     * `mode` makes it misbehave the way a test needs; what it swallowed is kept, so that a
     * test can deliver it later, as a device that answers after the host gave up does.
     */
    class scripted_device
    {
    public:
        std::vector< std::uint8_t > operator()( std::span< const std::uint8_t > written )
        {
            received_.push( written.data(), written.size() );

            if ( receiver_.receive() != receive_result::frame )
                return {};

            std::array< std::uint8_t, 64 >  storage = {};
            buffer_sink                     response( storage );

            if ( !dispatch_.dispatch( receiver_.payload(), 0, response ) )
                return {};

            std::vector< std::uint8_t > frame = framed( { storage.data(), response.size() } );

            if ( mode == behaviour::corrupt )
                frame.back() ^= 0xff;

            if ( mode == behaviour::swallow )
            {
                swallowed_ = std::move( frame );

                return {};
            }

            return frame;
        }

        /**
         * @brief the answer the device kept to itself while it swallowed
         */
        const std::vector< std::uint8_t >& swallowed() const
        {
            return swallowed_;
        }

        behaviour mode = behaviour::answer;

    private:
        using buffer_t = ring_buffer< std::uint8_t, 2 * ( default_max_payload + frame_overhead ) >;

        static std::vector< std::uint8_t > framed( std::span< const std::uint8_t > payload )
        {
            buffer_t                 buffer;
            frame_sender< buffer_t > sender( buffer );

            BOOST_REQUIRE( sender.send( payload ) );

            std::vector< std::uint8_t > bytes( buffer.available() );
            buffer.pop( bytes.data(), bytes.size() );

            return bytes;
        }

        instrument                                      device_;
        dispatcher< functions, instrument >             dispatch_{ device_ };
        buffer_t                                        received_;
        frame_receiver< default_max_payload, buffer_t > receiver_{ received_ };
        std::vector< std::uint8_t >                     swallowed_;
    };

    /*
     * The host end on a driven stream, with that device behind it.
     */
    struct fixture
    {
        boost::asio::io_context                                 io;
        driven_stream                                           stream{ io };
        scripted_device                                         device;
        stream_transport< driven_stream >                       transport{ io, stream, "the device", timeout };
        proxy< functions, stream_transport< driven_stream > >   remote{ transport };

        fixture()
        {
            stream.answers_with( [ this ]( std::span< const std::uint8_t > written ) { return device( written ); } );
        }
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
    device.mode = behaviour::swallow;
    BOOST_CHECK_THROW( remote.call< &instrument::add >( 1, 2 ), link_error );

    // the answer to that request, on the line after the host gave up on it
    stream.arrives( device.swallowed() );

    device.mode = behaviour::answer;
    BOOST_CHECK_EQUAL( remote.call< &instrument::add >( 10, 20 ), 30 );
}

namespace {

    struct driven
    {
        boost::asio::io_context                 io;
        driven_stream                           stream{ io };
        stream_transport< driven_stream >       transport{ io, stream, "the device", timeout };
    };

    // `payload` in a frame, as a device would send it
    std::vector< std::uint8_t > frame_of( std::initializer_list< std::uint8_t > payload )
    {
        using buffer_t = ring_buffer< std::uint8_t, 64 >;

        buffer_t                 buffer;
        frame_sender< buffer_t > sender( buffer );

        BOOST_REQUIRE( sender.send( { std::data( payload ), payload.size() } ) );

        std::vector< std::uint8_t > bytes( buffer.available() );
        buffer.pop( bytes.data(), bytes.size() );

        return bytes;
    }

    const std::uint8_t a_request[] = { 0x07, 0x11, 0x22 };
}

/*
 * A request that goes unanswered may have been swallowed by a frame the device is waiting for,
 * which a length read from noise announced; the transport ends that frame, so that the link is
 * usable again. What ends it is one frame's worth of 0xff, which
 * link_frame_tests.cpp shows frees a receiver whatever length it read.
 */
BOOST_FIXTURE_TEST_CASE( a_request_that_goes_unanswered_ends_a_frame_with_ones, driven )
{
    BOOST_CHECK_THROW( transport.transact( a_request ), link_error );

    const std::vector< std::uint8_t >& written = stream.written();
    const std::size_t                  ones    = default_max_payload + frame_overhead;

    BOOST_REQUIRE_GE( written.size(), ones );
    BOOST_CHECK( std::all_of( written.end() - ones, written.end(), []( std::uint8_t byte ) { return byte == 0xff; } ) );
}

// a link that answers is left alone
BOOST_FIXTURE_TEST_CASE( a_request_that_is_answered_ends_no_frame, driven )
{
    stream.answers_with( []( std::span< const std::uint8_t > ) { return frame_of( { 0xaa, 0xbb } ); } );

    BOOST_TEST( transport.transact( a_request ) == std::vector< std::uint8_t >( { 0xaa, 0xbb } ),
        boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( stream.written().size(), sizeof( a_request ) + frame_overhead );
}

BOOST_AUTO_TEST_CASE( a_serial_device_that_does_not_exist_cannot_be_opened )
{
    BOOST_CHECK_THROW( serial_transport( "/dev/bluetoe-does-not-exist", timeout ), rig_error );
}
