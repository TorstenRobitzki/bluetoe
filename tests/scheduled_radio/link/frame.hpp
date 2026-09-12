#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_FRAME_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_FRAME_HPP

/**
 * @file frame.hpp
 *
 * The frames of the link, see documentation/scheduled_radio_test_rig.md, decision 16. A
 * frame is
 *
 *     length | payload | crc
 *
 * with the length of the payload as 16 bits little endian, and a CRC-16 over the length
 * and the payload, little endian. Nothing marks the start of a frame: under decision 6
 * exactly one request is in flight, so after a corrupt frame the receiver discards
 * everything it holds, the host sees a link error, and its next request arrives into an
 * empty buffer.
 *
 * Both ends work on the rig's ring buffers and never block. The receiver consumes bytes as
 * they arrive and reports a frame only when its checksum has been verified. The sender
 * writes a frame only if the whole of it fits, so that a response that does not fit waits
 * for the next iteration rather than going out in part.
 */

#include "link/crc16.hpp"
#include "link/serial_port.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief bytes a frame adds to its payload
     */
    constexpr std::size_t frame_overhead = 4;

    /**
     * @brief the payload bound the rig and the host agree on unless told otherwise
     */
    constexpr std::size_t default_max_payload = 256;

    enum class receive_result
    {
        /**
         * @brief not all of the frame has arrived yet
         */
        incomplete,

        /**
         * @brief a frame with a valid checksum is available through payload()
         */
        frame,

        /**
         * @brief a frame failed its checksum or exceeded the maximum payload
         *
         * Everything the receive buffer held has been discarded.
         */
        corrupt
    };

    /**
     * @brief takes frames out of a receive buffer
     */
    template < std::size_t MaxPayload, byte_ring_buffer Buffer >
    class frame_receiver
    {
    public:
        explicit frame_receiver( Buffer& buffer )
            : buffer_( buffer ), size_( 0 ), crc_( 0 ) {}

        /**
         * @brief consume what has arrived and report whether it completed a frame
         *
         * Called from the application context, once per iteration of the main loop.
         */
        receive_result receive()
        {
            if ( !length_ )
            {
                if ( buffer_.available() < sizeof( std::uint16_t ) )
                    return receive_result::incomplete;

                std::uint8_t length[ sizeof( std::uint16_t ) ];
                buffer_.pop( length, sizeof( length ) );

                length_ = static_cast< std::uint16_t >( length[ 0 ] | ( length[ 1 ] << 8 ) );

                if ( *length_ > MaxPayload )
                    return discard();

                crc_ = crc16( length, sizeof( length ) );
            }

            if ( buffer_.available() < *length_ + sizeof( std::uint16_t ) )
                return receive_result::incomplete;

            buffer_.pop( payload_.data(), *length_ );
            crc_ = crc16( payload_.data(), *length_, crc_ );

            std::uint8_t crc[ sizeof( std::uint16_t ) ];
            buffer_.pop( crc, sizeof( crc ) );

            if ( crc_ != ( crc[ 0 ] | ( crc[ 1 ] << 8 ) ) )
                return discard();

            size_ = *length_;
            length_.reset();

            return receive_result::frame;
        }

        /**
         * @brief forget a partial frame and everything the buffer holds
         *
         * For a receiver that gave up waiting for a frame and does not want the remains
         * of it in front of the next one.
         */
        void reset()
        {
            discard();
        }

        /**
         * @brief the payload of the frame the last receive() reported
         *
         * Valid until the next call to receive().
         */
        std::span< const std::uint8_t > payload() const
        {
            return { payload_.data(), size_ };
        }

    private:
        receive_result discard()
        {
            for ( std::size_t left = buffer_.available(); left != 0; left = buffer_.available() )
            {
                std::uint8_t sink[ 16 ];
                const std::size_t count = left < sizeof( sink ) ? left : sizeof( sink );
                buffer_.pop( sink, count );
            }

            length_.reset();
            size_ = 0;

            return receive_result::corrupt;
        }

        Buffer&                                 buffer_;
        std::array< std::uint8_t, MaxPayload >  payload_;

        // the length of the frame being received, once its first two bytes arrived
        std::optional< std::uint16_t >          length_;
        std::size_t                             size_;
        std::uint16_t                           crc_;
    };

    /**
     * @brief puts frames into a transmit buffer
     */
    template < byte_ring_buffer Buffer >
    class frame_sender
    {
    public:
        explicit frame_sender( Buffer& buffer )
            : buffer_( buffer ) {}

        /**
         * @brief write one frame around `payload`
         *
         * @pre payload.size() fits into the 16 bit length; a payload that does not is a
         *      bug in the caller, not a condition of the link.
         *
         * @return false, and nothing written, if the frame does not fit into the buffer
         *         right now.
         */
        bool send( std::span< const std::uint8_t > payload )
        {
            assert( payload.size() <= 0xffff );

            if ( buffer_.free() < payload.size() + frame_overhead )
                return false;

            const std::uint8_t length[] = {
                static_cast< std::uint8_t >( payload.size() ),
                static_cast< std::uint8_t >( payload.size() >> 8 )
            };

            const std::uint16_t crc = crc16( payload.data(), payload.size(), crc16( length, sizeof( length ) ) );

            const std::uint8_t checksum[] = {
                static_cast< std::uint8_t >( crc ),
                static_cast< std::uint8_t >( crc >> 8 )
            };

            buffer_.push( length, sizeof( length ) );
            buffer_.push( payload.data(), payload.size() );
            buffer_.push( checksum, sizeof( checksum ) );

            return true;
        }

    private:
        Buffer& buffer_;
    };
}
}

#endif
