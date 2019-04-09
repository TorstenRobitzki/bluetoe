#ifndef BLUETOE_LINK_LAYER_RING_BUFFER_HPP
#define BLUETOE_LINK_LAYER_RING_BUFFER_HPP

#include <cstdint>
#include <cassert>
#include <cstdlib>

#include "buffer.hpp"
#include <bluetoe/default_pdu_layout.hpp>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief structure, able to store variable sized link layer PDUs in a fixed size ring.
     *
     * The buffer uses the size field of the stored PDUs as pointers
     * to the next PDU stored in the buffer.
     *
     * Elements are inserted at the front and removed from the end. When the ring buffer is empty
     * it is garantied that the buffer can store one elemente of at least Size - 1 in size.
     *
     * The Layout is used to access the header field of the PDU and to determin the in memory
     * length of stored PDUs.
     */
    template < std::size_t Size, typename Buffer = read_buffer, typename Layout = default_pdu_layout >
    class pdu_ring_buffer
    {
    public:
        /**
         * @brief the size of the buffer in bytes
         */
        static constexpr std::size_t size = Size;

        /**
         * @brief sets up the ring to be empty
         * @pre next_end().size == 0
         * @pre buffer must point to an array of at least Size bytes
         */
        explicit pdu_ring_buffer( std::uint8_t* buffer );

        /**
         * @brief resets the ring to be empty
         * @pre next_end().size == 0
         * @pre buffer must point to an array of at least Size bytes
         */
        void reset( std::uint8_t* buffer );

        /**
         * @brief return a writeable PDU buffer of at least size bytes at the front of the ring
         *
         * The function is idempotent and will yield the same results as long as the state of the
         * buffer is not changed and the parameters are the same.
         *
         * If there is not enough room for size bytes in the ring buffer, the function will return an
         * empty read_buffer.
         *
         * @pre size > Layout::data_channel_pdu_memory_size( 0 )
         * @pre buffer must point to an array of at least Size bytes
         */
        Buffer alloc_front( std::uint8_t* buffer, std::size_t size ) const;

        /**
         * @brief stores the allocated PDU in the ring.
         *
         * The length field of the PDU must contain the actual size of the PDU - 2.
         * Now the stored PDU can be read through the ring.
         *
         * @pre pdu.size >= Layout::data_channel_pdu_memory_size( Layout::header( pdu ) >> 8 )
         * @pre Layout::header( pdu ) >> 8 != 0
         * @post next_end().size != 0
         */
        void push_front( std::uint8_t* buffer, const Buffer& pdu );

        /**
         * @brief returns the next PDU from the ring.
         *
         * If no PDU is stored in the ring, the function will return an empty write_buffer.
         */
        Buffer next_end() const;

        /**
         * @brief frees the last PDU at the end of the ring
         *
         * @pre next_end().size == 0
         */
        void pop_end( std::uint8_t* buffer );

        /**
         * @brief returns true, if the buffer contains at least 2 elements
         */
        bool more_than_one() const;

    private:
        static constexpr std::size_t    ll_header_size = 2;
        static constexpr std::uint16_t  wrap_mark = 0;

        template < class P >
        static std::uint8_t pdu_length( const P& pdu );

        template < typename P >
        static std::size_t pdu_length( P* );

        // 1) if end_ == front_, the ring is empty
        //        end_ and front_ can point to everywhere into the buffer
        // 2) if front_ > end_, the all elements are between front_ and end_
        // 3) if end_ > front_, -> buffer splited
        //        there are elements from front_ to the end of the buffer
        //        and there are elements from the beginning of the buffer till end_
        std::uint8_t* end_;
        std::uint8_t* front_;
    };

    template < std::size_t Size, typename Buffer, typename Layout >
    pdu_ring_buffer< Size, Buffer, Layout >::pdu_ring_buffer( std::uint8_t* buffer )
    {
        reset( buffer );
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    void pdu_ring_buffer< Size, Buffer, Layout >::reset( std::uint8_t* buffer )
    {
        assert( buffer );
        front_ = buffer;
        end_   = buffer;

        Layout::header( buffer, wrap_mark );
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    Buffer pdu_ring_buffer< Size, Buffer, Layout >::alloc_front( std::uint8_t* buffer, std::size_t size ) const
    {
        assert( buffer );
        assert( size >= Layout::data_channel_pdu_memory_size( 0 ) );

        // buffer splited? There must be one byte left to not overflow the ring.
        if ( end_ > front_ && static_cast< std::ptrdiff_t >( size ) < end_ - front_ )
        {
            return Buffer{ front_, size };
        }

        if ( front_ >= end_ )
        {
            const std::uint8_t* end_of_buffer = buffer + Size;

            // allocate at the end?
            if ( static_cast< std::ptrdiff_t >( size ) <= end_of_buffer - front_ )
            {
                return Buffer{ front_, size };
            }

            // allocate at the begining? Again, there must be one byte left between the end front_ and the end_
            if ( static_cast< std::ptrdiff_t >( size ) < end_ - buffer )
            {
                return Buffer{ buffer, size };
            }
        }

        return Buffer{ 0, 0 };
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    void pdu_ring_buffer< Size, Buffer, Layout >::push_front( std::uint8_t* buffer, const Buffer& pdu )
    {
        assert( pdu.size >= pdu_length( pdu ) );

        const std::uint8_t* end_of_buffer = buffer + Size;

        // set size to 0 to mark force the end_ pointer to wrap here
        if ( front_ != pdu.buffer && front_ + 1 < end_of_buffer )
        {
            Layout::header( front_, wrap_mark );
        }

        const bool was_empty = front_ == end_;

        front_ = pdu.buffer + pdu_length( pdu );

        // if the ring was empty, the end_ ptr have to wrap too now
        if ( was_empty )
            end_ = pdu.buffer;
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    Buffer pdu_ring_buffer< Size, Buffer, Layout >::next_end() const
    {
        return front_ == end_
            ? Buffer{ 0, 0 }
            : Buffer{ end_, pdu_length( end_ ) };
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    void pdu_ring_buffer< Size, Buffer, Layout >::pop_end( std::uint8_t* buffer )
    {
        end_ += pdu_length( end_ );

        const std::uint8_t* end_of_buffer = buffer + Size;

        // wrap the end_ pointer to the beginning, if the buffer is not empty
        if ( end_ != front_ && ( end_ + 1 >= end_of_buffer || end_[ 1 ] == wrap_mark ) )
            end_ = buffer;
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    template < class P >
    std::uint8_t pdu_ring_buffer< Size, Buffer, Layout >::pdu_length( const P& pdu )
    {
        return pdu_length( pdu.buffer );
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    template < typename P >
    std::size_t pdu_ring_buffer< Size, Buffer, Layout >::pdu_length( P* p )
    {
        return Layout::data_channel_pdu_memory_size( Layout::header( p ) >> 8 );
    }

    template < std::size_t Size, typename Buffer, typename Layout >
    bool pdu_ring_buffer< Size, Buffer, Layout >::more_than_one() const
    {
        return end_ != front_ && ( end_ + pdu_length( end_) ) != front_;
    }

}
}

#endif
