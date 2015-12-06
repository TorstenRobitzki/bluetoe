#ifndef BLUETOE_LINK_LAYER_BUFFER_HPP
#define BLUETOE_LINK_LAYER_BUFFER_HPP

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <initializer_list>
#include <algorithm>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief type suitable to store the location and size of a chunk of
     *        memory that can be used to receive from the radio
     */
    struct read_buffer
    {
        /**
         * Points to the location where content of the buffer is located.
         * If size is 0, the value is unspecified.
         * The allignment of the address is unspecified.
         */
        std::uint8_t*   buffer;

        /**
         * Size of the chunk. If size is 0, an empty buffer is indicated and the value of buffer
         * is unspecified
         */
        std::size_t     size;

        /**
         * @brief returns true, if the buffer is empty
         */
        bool empty() const
        {
            return buffer == nullptr && size == 0;
        }

        /**
         * @brief copies the given data into the buffer
         * @pre data.size() <= size
         */
        void fill( std::initializer_list< std::uint8_t > data )
        {
            assert( data.size() <= size );
            std::copy( data.begin(), data.end(), buffer );
        }
    };

    /**
     * @brief type suitable to store the location and size of a chunk of
     *        memory that can be used to transmit to the radio
     */
    struct write_buffer
    {
        /** @copydoc read_buffer::buffer */
        const std::uint8_t* buffer;

        /** @copydoc read_buffer::size */
        std::size_t         size;

        /**
         * @brief returns true, if the buffer is empty
         */
        bool empty() const
        {
            return buffer == nullptr && size == 0;
        }

        /**
         * @brief constructs a ready-only buffer from a writable buffer
         */
        explicit write_buffer( const read_buffer& rhs )
            : buffer( rhs.buffer )
            , size( rhs.size )
        {}

        /**
         * @brief constructs an empty buffer
         */
        write_buffer()
            : buffer( nullptr )
            , size( 0 )
        {
        }

        /**
         * @brief constructs a buffer pointing to the given location, with the given size
         */
        write_buffer( const std::uint8_t* b, std::size_t s )
            : buffer( b )
            , size( s )
        {
        }
    };
}
}

#endif
