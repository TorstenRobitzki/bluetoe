#ifndef BLUETOE_LINK_LAYER_BUFFER_HPP
#define BLUETOE_LINK_LAYER_BUFFER_HPP

#include <cstdlib>
#include <cstdint>
#include <cassert>

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
    };

    /**
     * @brief ring buffer for ingoing and outgoing LL Data PDUs
     *
     * The buffer has two modes:
     * - Stopped mode: In this mode, the internal buffer can be accessed
     * - running mode: The buffer is used a to communicate between the link layer and the scheduled radio
     *
     * The buffer has three interfaces:
     * - one to access the transmit buffer from the link layer
     * - one to access the receive buffer from the link layer
     * - one to access the both buffers from the radio hardware
     *
     * This type is intendet to be inherited by the scheduled radio so that the
     * ll_data_pdu_buffer can access the nessary radio interface by casting this to Radio*
     * and to allow Radio to access the protected interface.
     */
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    class ll_data_pdu_buffer
    {
    public:
        /**
         * @post buffer is in stopped mode
         */
        ll_data_pdu_buffer();

        /**
         * @brief the size of memory in bytes that are return by raw()
         */
        static constexpr std::size_t    size            = TransmitSize + ReceiveSize;
        static constexpr std::size_t    min_buffer_size = 27;
        static constexpr std::size_t    max_buffer_size = 251;

        /**@{*/
        /**
         * @name buffer sizes
         */

        /**
         * @brief returns the maximum value that can be used as maximum receive size.
         *
         * The result is equal to ReceiveSize.
         */
        constexpr std::size_t max_max_rx_size() const
        {
            return ReceiveSize;
        }

        /**
         * @brief the current maximum receive size
         *
         * No PDU with larger size can be receive. This will always return at least 27.
         */
        std::size_t max_rx_size() const;

        /**
         * @brief set the maximum receive size
         *
         * The used size must be smaller or equal to ReceiveSize / max_max_rx_size(), smaller than 252 and larger or equal to 27.
         * The memory is best used, when ReceiveSize divided by max_size results in an integer. That integer is
         * then the number of PDUs that can be buffered on the receivin side.
         *
         * By default the function will return 27.
         *
         * @post max_rx_size() == max_size
         */
        void max_rx_size( std::size_t max_size );

        /**
         * @brief returns the maximum value that can be used as maximum receive size.
         *
         * The result is equal to TransmitSize.
         */
        constexpr std::size_t max_max_tx_size() const
        {
            return TransmitSize;
        }

        /**
         * @brief the current maximum transmit size
         *
         * No PDU with larger size can be transmitted. This will always return at least 27.
         */
        std::size_t max_tx_size() const;

        /**
         * @brief set the maximum transmit size
         *
         * The used size must be smaller or equal to TransmitSize / max_max_tx_size(), smaller than 252 and larger or equal to 27.
         * The memory is best used, when TransmitSize divided by max_size results in an integer. That integer is
         * then the number of PDUs that can be buffered on the transmitting side.
         *
         * By default the function will return 27.
         *
         * @post max_tx_size() == max_size
         */
        void max_tx_size( std::size_t max_size );

        /**@}*/

        /**@{*/
        /**
         * @name buffer modes
         */

        /**
         * @brief returns the underlying raw buffer with a size of at least ll_data_pdu_buffer::size
         *
         * The underlying memory can be used when it's not used by other means. The size that can saftely be used is ll_data_pdu_buffer::size.
         * @pre buffer is in stopped mode
         */
        std::uint8_t* raw();

        /**
         * @brief places the buffer in stopped mode.
         *
         * If the buffer is in stopped mode, it's internal memory can be used for other
         * purposes.
         */
        void stop();

        /**
         * @brief places the buffer in running mode.
         *
         * Receive and transmit buffers are empty. Sequence numbers are reseted.
         * max_rx_size() is set to default
         *
         * @post next_received().empty()
         * @post max_rx_size() == 27u
         */
        void reset();

        void schedule_next_connection_event();

        /**@}*/

        /**@{*/
        /**
         * @name Interface to access the transmit buffers from the link layer
         */

        /**
         * @brief allocates a certain amount of memory to place a PDU to be transmitted.
         *
         * If not enough memory is available, the function will return an empty buffer (size == 0).
         * To indicate that the allocated memory is filled with data to be send, commit_transmit_buffer() must be called.
         *
         * @post r = allocate_transmit_buffer( n ); r.size == 0 || r.size == n
         * @pre  buffer is in running mode
         * @pre size <= max_tx_size()
         */
        read_buffer allocate_transmit_buffer( std::size_t size );

        /**
         * @brief calles allocate_transmit_buffer( max_tx_size() );
         */
        read_buffer allocate_transmit_buffer();

        /**
         * @brief indicates that prior allocated memory is now ready for transmission
         *
         * To send a PDU, first allocate_transmit_buffer() have to be called, with the maximum size
         * need to assemble the PDU. Then commit_transmit_buffer() have to be called with the
         * size that the PDU is really filled with at the begining of the buffer.
         *
         * @pre a buffer must have been allocated by a call to allocate_transmit_buffer()
         * @pre size
         * @pre buffer is in running mode
         */
        void commit_transmit_buffer( read_buffer );

        /**@}*/

        /**@{*/
        /**
         * @name Interface to access the receive buffer from the link layer
         */

        /**
         * @brief returns the oldest PDU out of the receive buffer.
         *
         * If there is no PDU in the receive buffer, an empty PDU will be returned (size == 0 ).
         * The returned PDU is not removed from the buffer. To remove the buffer after it is
         * not used any more, free_received() must be called.
         *
         * @pre buffer is in running mode
         */
        write_buffer next_received() const;

        /**
         * @brief removes the oldest PDU from the receive buffer.
         * @pre next_received() was called without free_received() beeing called.
         * @pre buffer is in running mode
         */
        void free_received();

        /**@}*/

    protected:
        /**@{*/
        /**
         * @name Interface to the radio hardware
         */

        /**
         * @brief allocates a buffer for the next PDU to be received.
         *
         * Once a buffer was allocated to the radio hardware is will be released by the hardware by calling
         * on of received(), crc_error() or timeout().
         *
         * This function can return an empty buffer if the receive buffers are all still allocated. The radio is
         * than required to ignore all incoming trafic.
         *
         * @attention there should be a maximum of one allocated but jet not released buffer.
         */
        read_buffer allocate_receive_buffer();

        /**
         * @brief This function will be called by the scheduled radio when a PDU was received without error.
         *
         * The function returns the next buffer to be transmitted.
         */
        write_buffer received( read_buffer );

        /**
         * @brief This function will be called by the scheduled radio when a PDU was received with CRC error
         */
        write_buffer crc_error();

        /**
         * @brief This function will be called by the scheduled radio when a timeout occured.
         */
        void timeout();

        /**@}*/

    private:
        // transmit buffer followed by receive buffer at buffer_[ TransmitSize ]
        std::uint8_t    buffer_[ size + 2 ];

        // pointers into the receive buffer. received_ will point to the oldest not yet freed
        // element. The LL Data PDU length field ( offset 1 ) will give a pointer to next element
        // until received_end_ is reached. If a length field is filled with 0, the next element
        // starts at the beginning of the buffer. If the next length field is outside the buffer,
        // the next element starts at the beginning of the buffer.
        std::uint8_t* volatile  received_;
        std::uint8_t* volatile  received_end_;

        volatile std::size_t    max_rx_size_;

        std::uint8_t* volatile  transmitted_;
        std::uint8_t* volatile  transmitted_end_;

        volatile std::size_t    max_tx_size_;

        static constexpr std::size_t ll_header_size = 2;

        std::uint8_t* transmit_buffer()
        {
            return &buffer_[ 0 ];
        }

        const std::uint8_t* transmit_buffer() const
        {
            return &buffer_[ 0 ];
        }

        std::uint8_t* end_transmit_buffer()
        {
            return &buffer_[ TransmitSize + 1 ];
        }

        const std::uint8_t* end_transmit_buffer() const
        {
            return &buffer_[ TransmitSize + 1 ];
        }

        std::uint8_t* receive_buffer()
        {
            return &buffer_[ TransmitSize + 1 ];
        }

        const std::uint8_t* receive_buffer() const
        {
            return &buffer_[ TransmitSize + 1 ];
        }

        std::uint8_t* end_receive_buffer()
        {
            return &buffer_[ size + 2 ];
        }

        const std::uint8_t* end_receive_buffer() const
        {
            return &buffer_[ size + 2 ];
        }

        bool check_receive_buffer_consistency( const char* label ) const;

    };

    // implementation
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::ll_data_pdu_buffer()
    {
        reset();
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    std::size_t ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::max_rx_size() const
    {
        return max_rx_size_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::max_rx_size( std::size_t max_size )
    {
        assert( max_size >= min_buffer_size );
        assert( max_size <= max_buffer_size );
        assert( max_size <= ReceiveSize );

        max_rx_size_ = max_size;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    std::size_t ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::max_tx_size() const
    {
        return max_tx_size_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::max_tx_size( std::size_t max_size )
    {
        assert( max_size >= min_buffer_size );
        assert( max_size <= max_buffer_size );
        assert( max_size <= TransmitSize );

        max_tx_size_ = max_size;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::reset()
    {
        max_rx_size_  = min_buffer_size;
        received_end_ = receive_buffer();
        received_     = receive_buffer();

        max_tx_size_  = min_buffer_size;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    std::uint8_t* ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::raw()
    {
        return &buffer_[ 0 ];
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_transmit_buffer( std::size_t )
    {
        return read_buffer{ transmit_buffer() , max_tx_size_ };
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_transmit_buffer()
    {
        return allocate_transmit_buffer( max_tx_size_ );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::commit_transmit_buffer( read_buffer )
    {
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::next_received() const
    {
        return received_ != received_end_
            ? write_buffer{ received_, received_[ 1 ] + ll_header_size }
            : write_buffer{ 0, 0 };
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::free_received()
    {
        assert( received_ != received_end_ );
        assert( check_receive_buffer_consistency( "+free_received" ) );

        received_ += received_[ 1 ] + ll_header_size;

        // do we have to wrap?
        if ( end_receive_buffer() - received_ < 2 || received_[ 1 ] == 0 )
        {
            if ( received_ == received_end_ )
                received_end_ = receive_buffer();

            received_ = receive_buffer();
        }

        assert( check_receive_buffer_consistency( "-free_received" ) );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    bool ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::check_receive_buffer_consistency( const char* label ) const
    {
        if ( received_ == received_end_ )
            return true;

        const std::uint8_t* c = received_;

        if ( received_ < received_end_ )
        {
            // now, by following the length field, we have to reach received_end_ whithout wrapping
            while ( c < received_end_ )
                c += c[ 1 ] + ll_header_size;

            return c == received_end_;
        }

        // now by following the length field, we have to reach the end of the buffer
        while ( c != receive_buffer() && c < end_receive_buffer() )
        {
            c += c[ 1 ] + ll_header_size;

            if ( c > end_receive_buffer() )
                return false;

            if ( end_receive_buffer() - c < 2 || c[ 1 ] == 0 )
                c = receive_buffer();
        }

        if ( c != receive_buffer() )
            return false;

        // wrap around and have to meet received_end_
        while ( c < received_end_ )
            c += c[ 1 ] + ll_header_size;

        return c == received_end_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_receive_buffer()
    {
        // The gap must be greater than max_rx_size_ or otherwise we could endup with
        // received_ == received_end_ without the buffer beeing empty
        if ( received_end_ >= received_ )
        {
            // place a 0 into the length field to denote that the rest until the buffer end is not used
            if ( end_receive_buffer() - received_end_ >= ll_header_size )
            {
                received_end_[ 1 ] = 0;
            }

            // is there still a gap at the end of the buffer?
            if ( end_receive_buffer() - received_end_ > max_rx_size_ )
            {
                return read_buffer{ received_end_, max_rx_size_ };
            }

            // is there a gap at the beginning of the buffer?
            if ( received_ - receive_buffer() > max_rx_size_ )
            {
                return read_buffer{ receive_buffer(), max_rx_size_ };
            }
        }
        // the filled part of the buffer is wrapped around the end; is there a gap in the middle?
        else if ( received_ - received_end_ > max_rx_size_ )
        {
            return read_buffer{ received_end_, max_rx_size_ };
        }

        return read_buffer{ nullptr, 0 };
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::received( read_buffer pdu )
    {
        assert( pdu.buffer );
        assert( pdu.buffer >= receive_buffer() );
        assert( pdu.size >= pdu.buffer[ 1 ] + ll_header_size );
        assert( end_receive_buffer() - pdu.buffer >= pdu.buffer[ 1 ] + ll_header_size );
        assert( check_receive_buffer_consistency( "+received" ) );

        if ( pdu.buffer[ 1 ] != 0 )
        {
            // TODO: Flow control
            received_end_ = pdu.buffer + pdu.buffer[ 1 ] + ll_header_size;

            // we received data, so we can not end up with the receive buffer beeing empty
            assert( received_end_ != received_ );
        }

        assert( check_receive_buffer_consistency( "-received" ) );

        // TODO: current element from transmit buffer
        return write_buffer{ 0, 0 };
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::crc_error()
    {
        return write_buffer{ 0, 0 };
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::timeout()
    {
    }

}
}

#endif
