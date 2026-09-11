#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_STREAM_TRANSPORT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_STREAM_TRANSPORT_HPP

/**
 * @file stream_transport.hpp
 *
 * The host's end of the link over any Boost.Asio stream: a request written as a frame,
 * the response read as one, within a timeout. This is the transport of proxy.hpp;
 * serial_transport.hpp binds it to a serial port, the unit tests to a socket.
 */

#include "host/errors.hpp"
#include "link/frame.hpp"
#include "link/ring_buffer.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief request and response frames over an Asio stream
     *
     * Stream is an Asio stream with async_read_some(), synchronous write and cancel(),
     * such as a serial port or a socket. The stream and the io_context outlive the
     * transport, and nobody else runs the io_context.
     */
    template < typename Stream >
    class stream_transport
    {
    public:
        /**
         * @param name   how the stream is called in error messages
         * @param timeout how long the response to one request may take to arrive in
         *               full; a toolbox call is answered from inside the dispatcher, so
         *               for a device with a toolbox this covers a point multiplication
         */
        stream_transport( boost::asio::io_context& io, Stream& stream, std::string name, std::chrono::milliseconds timeout )
            : io_( io )
            , stream_( stream )
            , name_( std::move( name ) )
            , timeout_( timeout )
            , timer_( io )
            , receiver_( received_ )
            , sender_( to_send_ )
        {
        }

        /**
         * @brief writes the request as a frame and returns the payload of the response frame
         *
         * Whatever the stream still holds from an earlier request is discarded first: a
         * response carries no request id, so a late answer to a request that timed out
         * must not be taken for the answer to this one. Decision 6 keeps at most one
         * request in flight, which is what makes discarding safe.
         *
         * @throws link_error no response within the timeout, a corrupt response, or a
         *                    stream error
         */
        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            discard_stale();
            write( request );

            return read_response();
        }

    private:
        using buffer_t = ring_buffer< std::uint8_t, 2 * ( default_max_payload + frame_overhead ) >;

        /*
         * Reads what has already arrived, until a non-blocking poll finds nothing.
         */
        void discard_stale()
        {
            for ( ;; )
            {
                bool read_done = false;

                stream_.async_read_some( boost::asio::buffer( chunk_ ), [ & ]( const boost::system::error_code&, std::size_t ) {
                    read_done = true;
                } );

                io_.restart();
                io_.poll();

                if ( !read_done )
                {
                    stream_.cancel();
                    io_.restart();
                    io_.run();

                    break;
                }
            }

            receiver_.reset();
        }

        void write( std::span< const std::uint8_t > request )
        {
            if ( !sender_.send( request ) )
                throw link_error( "request does not fit into a frame" );

            std::vector< std::uint8_t > bytes( to_send_.available() );
            to_send_.pop( bytes.data(), bytes.size() );

            try
            {
                boost::asio::write( stream_, boost::asio::buffer( bytes ) );
            }
            catch ( const boost::system::system_error& error )
            {
                throw link_error( "cannot write to " + name_ + ": " + error.what() );
            }
        }

        std::vector< std::uint8_t > read_response()
        {
            bool timed_out = false;

            timer_.expires_after( timeout_ );
            timer_.async_wait( [ & ]( const boost::system::error_code& error ) {
                if ( !error )
                {
                    timed_out = true;
                    stream_.cancel();
                }
            } );

            for ( ;; )
            {
                switch ( receiver_.receive() )
                {
                    case receive_result::frame:
                        stop_timer();
                        return { receiver_.payload().begin(), receiver_.payload().end() };

                    case receive_result::corrupt:
                        stop_timer();
                        throw link_error( "corrupt response frame from " + name_ );

                    case receive_result::incomplete:
                        break;
                }

                boost::system::error_code   read_error;
                std::size_t                 read      = 0;
                bool                        read_done = false;

                stream_.async_read_some( boost::asio::buffer( chunk_ ), [ & ]( const boost::system::error_code& error, std::size_t size ) {
                    read_error = error;
                    read       = size;
                    read_done  = true;
                } );

                // one of the two handlers ends the wait: the read, or the timer cancelling it
                io_.restart();

                while ( !read_done )
                    io_.run_one();

                if ( timed_out )
                    throw link_error( "no response from " + name_ + " within " + std::to_string( timeout_.count() ) + " ms" );

                if ( read_error )
                    throw link_error( "cannot read from " + name_ + ": " + read_error.message() );

                if ( received_.free() < read )
                    throw link_error( "response from " + name_ + " longer than a frame" );

                received_.push( chunk_.data(), read );
            }
        }

        /*
         * Cancels the timer and lets its handler run, so that no handler is left over
         * for the next request.
         */
        void stop_timer()
        {
            timer_.cancel();
            io_.restart();
            io_.run();
        }

        boost::asio::io_context&                        io_;
        Stream&                                         stream_;
        std::string                                     name_;
        std::chrono::milliseconds                       timeout_;
        boost::asio::steady_timer                       timer_;
        buffer_t                                        received_;
        frame_receiver< default_max_payload, buffer_t > receiver_;
        buffer_t                                        to_send_;
        frame_sender< buffer_t >                        sender_;
        std::array< std::uint8_t, 256 >                 chunk_;
    };
}
}

#endif
