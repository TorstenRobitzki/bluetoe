#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_RING_BUFFER_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_RING_BUFFER_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a ring buffer with one producer and one consumer
     *
     * Holds up to Size elements of type T. One side only pushes, the other only pops, from
     * two different contexts; no call blocks or disables interrupts. Satisfies
     * byte_ring_buffer for T = std::uint8_t, and holds the rig's records the same way.
     *
     * free() and available() are lower bounds: the other side may have made more room, or
     * added more data, since the answer was computed, but never less, so each side acts on
     * the answer without further synchronisation. The producer owns write_, the consumer
     * owns read_, and each publishes its index only after its data is in place.
     *
     * The indices run over [0, 2 * Size) rather than [0, Size), so that a full buffer and an
     * empty one are distinguishable without wasting a slot, for any Size.
     */
    template < typename T, std::size_t Size >
    class ring_buffer
    {
    public:
        static constexpr std::size_t capacity = Size;

        ring_buffer() : read_( 0 ), write_( 0 ) {}

        ring_buffer( const ring_buffer& ) = delete;
        ring_buffer& operator=( const ring_buffer& ) = delete;

        /**
         * @brief number of elements that can be pushed right now, at least
         */
        std::size_t free() const
        {
            return Size - fill( write_.load( std::memory_order_relaxed ), read_.load( std::memory_order_acquire ) );
        }

        /**
         * @brief append elements
         *
         * @pre count <= free()
         */
        void push( const T* data, std::size_t count )
        {
            const std::size_t write = write_.load( std::memory_order_relaxed );
            assert( count <= Size - fill( write, read_.load( std::memory_order_acquire ) ) );

            copy_in( data, count, position( write ) );
            write_.store( advance( write, count ), std::memory_order_release );
        }

        /**
         * @brief number of elements that can be popped right now, at least
         */
        std::size_t available() const
        {
            return fill( write_.load( std::memory_order_acquire ), read_.load( std::memory_order_relaxed ) );
        }

        /**
         * @brief remove the oldest elements
         *
         * @pre count <= available()
         */
        void pop( T* out, std::size_t count )
        {
            const std::size_t read = read_.load( std::memory_order_relaxed );
            assert( count <= fill( write_.load( std::memory_order_acquire ), read ) );

            copy_out( out, count, position( read ) );
            read_.store( advance( read, count ), std::memory_order_release );
        }

    private:
        static std::size_t fill( std::size_t write, std::size_t read )
        {
            return write >= read ? write - read : write + 2 * Size - read;
        }

        static std::size_t position( std::size_t index )
        {
            return index >= Size ? index - Size : index;
        }

        static std::size_t advance( std::size_t index, std::size_t count )
        {
            const std::size_t next = index + count;

            return next >= 2 * Size ? next - 2 * Size : next;
        }

        void copy_in( const T* data, std::size_t count, std::size_t at )
        {
            const std::size_t first = std::min( count, Size - at );

            std::copy( data, data + first, storage_.begin() + at );
            std::copy( data + first, data + count, storage_.begin() );
        }

        void copy_out( T* out, std::size_t count, std::size_t at )
        {
            const std::size_t first = std::min( count, Size - at );

            std::copy( storage_.begin() + at, storage_.begin() + at + first, out );
            std::copy( storage_.begin(), storage_.begin() + ( count - first ), out + first );
        }

        std::array< T, Size >       storage_;
        std::atomic< std::size_t >  read_;
        std::atomic< std::size_t >  write_;
    };
}
}

#endif
