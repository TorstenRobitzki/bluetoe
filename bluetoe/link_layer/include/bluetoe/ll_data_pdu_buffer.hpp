#ifndef BLUETOE_LINK_LAYER_LL_DAAT_BUFFER_HPP
#define BLUETOE_LINK_LAYER_LL_DAAT_BUFFER_HPP

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <initializer_list>
#include <algorithm>

#include <bluetoe/default_pdu_layout.hpp>
#include "ring_buffer.hpp"

namespace bluetoe {
namespace link_layer {

    /**
     * @brief size book keeping and calculations
     *
     * The buffer have to store either link layer PDUs or L2CAP SDUs. For link layer control
     * PDUs, there is a minimum size of 29 bytes, that is defined by the set of supported LL control
     * procedures (29 -> Version 4.2). The minimum L2CAP size depends on the feature set. Without
     * LESC a MTU size of 23 is requires, with LESC, the minimum MTU size would be 65 (Vol 3, Part H, 3.2.)
     *
     * This class calculates the maximum LL PDU sizes that are possible with the given Buffer sizes and the
     * maxmim L2CAP SDUs that are possible with the given buffer sizes. It's then up to a different layer to
     * check that against the requirements of the given feature set.
     *
     * Currently, the underlying ring buffer can devide the used buffer in two ranges, even when the buffer is empty.
     * As a result of that, the buffer size needs to be at least double of the required size.
     */
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    class ll_data_buffer_sizes
    {
    public:
        ll_data_buffer_sizes();

        /**
         * @brief layout to be applied to each PDU.
         *
         * Basecally, this defined the overhead per PDU (layout_overhead).
         */
        using layout_t = Layout;

        /**
         * @brief the minimum size an element in the buffer can have (header size + payload size).
         */
        static constexpr std::size_t    min_ll_pdu_size = 29;

        /**
         * @brief the maximum size an element in the buffer can have (header size + payload size).
         */
        static constexpr std::size_t    max_ll_pdu_size = 255;

        static constexpr std::size_t    header_size     = 2u;
        static constexpr std::size_t    layout_overhead = layout_t::data_channel_pdu_memory_size( 0 ) - header_size;

        /**@{*/
        /**
         * @name link layer buffer sizes
         */

        /**
         * @brief returns the maximum link layer PDU receive size
         *
         * That is, the maximum PDU size that can be received through the link layer.
         * This will be at least 29 (to cover the minimum ATT MTU size of 23) and at maximum 255.
         */
        constexpr std::size_t max_ll_pdu_receive_size() const
        {
            return half_receive_size - layout_overhead  > max_ll_pdu_size
                ? max_ll_pdu_size
                : half_receive_size - layout_overhead;
        }

        /**
         * @brief the current maximum link layer PDU receive size
         *
         * No link layer PDU with larger size can be receive. This will always return at least 29 plus the Radio's layout overhead.
         */
        std::size_t current_ll_pdu_receive_size() const;

        /**
         * @brief set the maximum receive size
         *
         * The used size must be smaller or equal tp max_ll_pdu_receive_size, smaller than 251 and larger or equal to 29.
         * The memory is best used, when ReceiveSize divided by max_size + layout_overhead results in an integer. That integer is
         * then the number of PDUs that can be buffered on the receivin side.
         *
         * By default the function will return 29.
         *
         * @post current_ll_pdu_receive_size() == max_size
         */
        void current_ll_pdu_receive_size( std::size_t max_size );

        /**
         * @brief returns the maximum value that can be used as maximum receive size.
         *
         * The result is equal to TransmitSize.
         */
        constexpr std::size_t max_ll_pdu_transmit_size() const
        {
            return half_transmit_size - layout_overhead  > max_ll_pdu_size
                ? max_ll_pdu_size
                : half_transmit_size - layout_overhead;
        }

        /**
         * @brief the current maximum transmit size
         *
         * No PDU with larger size can be transmitted. This will always return at least 29.
         */
        std::size_t current_ll_pdu_transmit_size() const;

        /**
         * @brief set the maximum transmit size
         *
         * The used size must be smaller or equal to TransmitSize / max_max_tx_size(), smaller than 251 and larger or equal to 29.
         * The memory is best used, when TransmitSize divided by max_size results in an integer. That integer is
         * then the number of PDUs that can be buffered on the transmitting side.
         *
         * By default the function will return 29.
         *
         * @post current_ll_pdu_transmit_size() == max_size
         */
        void current_ll_pdu_transmit_size( std::size_t max_size );

        /**@}*/

        /**@{*/
        /**
         * @name L2CAP layer buffer sizes
         */

        /**
         * @brief the maximum buffer size that can be allocated for a (potentially) fragmented outgoing L2CAP SDU.
         *
         * As there is no guaranty that higher LL PDU sizes are used in the link layer, the function assumes,
         * that SDUs are fragemented using 29 byte large LL PDUs.
         */
        constexpr std::size_t max_l2cap_sdu_receive_size() const
        {
            // if L2CAP PDUs have to be fragmented, at worst, they have to be fragmented into small 29 byte LL PDUs
            // every fragment will add layout_overhead and header_size
            return min_ll_pdu_size + num_full_received_fragments * (min_ll_pdu_size - header_size) + last_received_fragment_size;
        }

        /**
         * @brief the maximum buffer size that can be allocated for a (potentially) fragmented incomming L2CAP SDU.
         *
         * As there is no guaranty that higher LL PDU sizes are used in the link layer, the function assumes,
         * that SDUs are fragemented using 29 byte large LL PDUs.
         */
        constexpr std::size_t max_l2cap_sdu_transmit_size() const
        {
            return min_ll_pdu_size + num_full_transmit_fragments * (min_ll_pdu_size - header_size) + last_transmit_fragment_size;
        }

        /**@}*/

        /**
         * @brief reset current buffer sizes to a minium.
         *
         * @post current_ll_pdu_receive_size() == 29u
         * @post current_ll_pdu_transmit_size() == 29u
         */
        void reset_buffer_sizes();

    private:
        /** @cond HIDDEN_SYMBOLS */
        // if size of the buffer is odd, the larger half is what limits the pdu size
        static constexpr std::size_t half_receive_size              = ( ReceiveSize + 1 ) / 2;
        static constexpr std::size_t half_transmit_size             = ( TransmitSize + 1 ) / 2;
        // The number of full PDUs in a fragmented L2CAP SDU (beside the first one)
        static constexpr std::size_t num_full_received_fragments    =
            ( half_receive_size - min_ll_pdu_size - layout_overhead ) / (min_ll_pdu_size + layout_overhead);
        static constexpr std::size_t num_full_transmit_fragments    =
            ( half_transmit_size - min_ll_pdu_size - layout_overhead ) / (min_ll_pdu_size + layout_overhead);
        // What remains, after storing the full fragments
        static constexpr std::size_t remaining_received_size        =
            half_receive_size - num_full_received_fragments * (min_ll_pdu_size + layout_overhead) - (min_ll_pdu_size + layout_overhead);
        static constexpr std::size_t remaining_transmit_size        =
            half_transmit_size - num_full_transmit_fragments * (min_ll_pdu_size + layout_overhead) - (min_ll_pdu_size + layout_overhead);
        // If the remaining size exceeds the header and layout overhead, it can be used to extend the fragmented L2CAP size
        static constexpr std::size_t last_received_fragment_size    =
            remaining_received_size > (header_size + layout_overhead)
                ? remaining_received_size - (header_size + layout_overhead)
                : 0;
        static constexpr std::size_t last_transmit_fragment_size    =
            remaining_transmit_size > (header_size + layout_overhead)
                ? remaining_transmit_size - (header_size + layout_overhead)
                : 0;
        /** @endcond */

        std::size_t current_ll_pdu_receive_size_;
        std::size_t current_ll_pdu_transmit_size_;
    };

    /**
     * @brief ring buffers for ingoing and outgoing LL Data PDUs and fragmented L2CAP SDUs
     *
     * The buffer has two modes:
     * - Stopped mode: In this mode, the internal buffer can be accessed
     * - running mode: The buffer is used to communicate between the link layer / L2CAP layer and the scheduled radio
     *
     * The buffer has 4 interfaces:
     * - one to access the transmit buffer from the link layer
     * - one to access the transmit buffer from the L2CAP layer
     * - one to access the receive buffer from the link layer / L2CAP layer
     * - one to access both buffers from the radio hardware
     *
     * This type is intendet to be inherited by the scheduled radio so that the
     * ll_data_pdu_buffer can access the nessary radio interface by casting this to Radio*
     * and to allow Radio to access the protected interface.
     *
     * TransmitSize and ReceiveSize are the total size of memory for the receiving and
     * transmitting buffer. Depending on the layout of the used Radio, there might be
     * an overhead per PDU.
     *
     * The buffer handles L2CAP fragmentation and defragmentation.
     *
     * While L2CAP SDUs are passed to the buffer at once. If the size of the
     * SDU is less than or equal to current_ll_pdu_transmit_size(), no fragmentation is required.
     */
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    class ll_data_pdu_buffer : public ll_data_buffer_sizes< TransmitSize, ReceiveSize, typename pdu_layout_by_radio< Radio >::pdu_layout >
    {
    public:
        using layout_t = typename pdu_layout_by_radio< Radio >::pdu_layout;

        /**
         * @post buffer is in stopped mode
         */
        ll_data_pdu_buffer();

        /**
         * @brief the size of memory in bytes that are return by raw()
         */
        static constexpr std::size_t    size            = TransmitSize + ReceiveSize;

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
         *
         * @post next_ll_received().empty()
         */
        void reset();

        /**@}*/

        /**@{*/
        /**
         * @name Interface to access the transmit buffers from the link layer
         */

        /**
         * @brief allocates a certain amount of memory to place a PDU to be transmitted .
         *
         * If not enough memory is available, the function will return an empty buffer (size == 0).
         * To indicate that the allocated memory is filled with data to be send, commit_transmit_buffer() must be called.
         * The size parameter is the sum of the payload + header.
         *
         * If the layout of the Radio requires additonal overhead, the returned buffer will be larger.
         *
         * @post r = allocate_ll_transmit_buffer( n ); r.size == 0 || r.size >= n
         * @pre  buffer is in running mode
         * @pre size <= max_tx_size()
         */
        read_buffer allocate_ll_transmit_buffer( std::size_t size );

        /**
         * @brief calls allocate_ll_transmit_buffer( current_ll_pdu_transmit_size() );
         */
        read_buffer allocate_ll_transmit_buffer();

        /**
         * @brief indicates that prior allocated memory is now ready for transmission
         *
         * To send a PDU, first allocate_ll_transmit_buffer() have to be called, with the maximum size
         * need to assemble the PDU. Then commit_ll_transmit_buffer() have to be called with the
         * size that the PDU is really filled with at the begining of the buffer.
         *
         * @pre a buffer must have been allocated by a call to allocate_ll_transmit_buffer()
         * @pre buffer is in running mode
         */
        void commit_ll_transmit_buffer( read_buffer );

        /**@}*/

        /**@{*/
        /**
         * @name Interface to access the transmit buffers from the L2CAP layer
         */

        /**
         * @brief allocates a L2CAP buffer, that can keep L2CAP buffers of the given size.
         *
         * If the buffer can not allocate a buffer large enough, to satisfy the request, an
         * empty buffer is returned.
         */
        read_buffer allocate_l2cap_transmit_buffer( std::size_t size );

        /**
         * @brief indicates that prior allocated memory is now ready for transmission
         *
         * To send a L2CAP SDU, first allocate_l2cap_transmit_buffer() have to be called, with the maximum size
         * need to assemble the SDU. Then commit_l2cap_transmit_buffer() have to be called with the
         * size that the SDU is really filled with at the begining of the buffer.
         *
         * @pre a buffer must have been allocated by a call to allocate_l2cap_transmit_buffer()
         * @pre buffer is in running mode
         */
        void commit_l2cap_transmit_buffer( read_buffer );

        /**@}*/

        /**@{*/
        /**
         * @name Interface to access the receive buffer from the link layer / L2CAP layer
         *
         * The differenciation between link layer and L2CAP layer have to be made on calling side.
         */

        /**
         * @brief returns the oldest link layer PDU or L2CAP SDU out of the receive buffer.
         *
         * If there is no oldest received PDU/SDU, an empty PDU will be returned (size == 0 ).
         * The returned PDU is not removed from the buffer. To remove the buffer after it is
         * not used any more, free_ll_l2cap_received() must be called.
         *
         * @pre buffer is in running mode
         *
         * @sa free_ll_l2cap_received()
         */
        write_buffer next_ll_l2cap_received() const;

        /**
         * @brief removes the oldest PDU from the receive buffer.
         * @pre next_ll_received() was called without free_ll_received() beeing called.
         * @pre buffer is in running mode
         */
        void free_ll_l2cap_received();

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
         * one of received(), crc_error() or timeout().
         *
         * This function can return an empty buffer if the receive buffers are all still allocated. The radio is
         * than required to ignore all incoming trafic.
         *
         * @attention there should be a maximum of one allocated but jet not released buffer.
         */
        read_buffer allocate_receive_buffer() const;

        /**
         * @brief This function will be called by the scheduled radio when a PDU was received without error.
         *
         * The function returns the next buffer to be transmitted.
         *
         * This function will call increment_receive_packet_counter() and increment_transmit_packet_counter() on
         * the Radio if the counter part of the encryption IV part have to be incremented.
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

        /**
         * @brief returns the next PDU to be transmitted
         *
         * If the transmit buffer is empty, the function will return an empty PDU.
         * @post next_transmit().size != 0 && next_transmit().buffer != nullptr
         */
        write_buffer next_transmit();

        /**@}*/

    private:
        // transmit buffer followed by receive buffer at buffer_[ TransmitSize ]
        std::uint8_t    buffer_[ size ];

        pdu_ring_buffer< ReceiveSize, read_buffer, layout_t >  receive_buffer_;
        volatile std::size_t            max_rx_size_;

        pdu_ring_buffer< TransmitSize, read_buffer, layout_t > transmit_buffer_;
        volatile std::size_t            max_tx_size_;

        bool                    sequence_number_;
        bool                    next_expected_sequence_number_;

        // an empty LL PDU to be send, if the transmission buffer is empty
        uint8_t                 empty_[ layout_t::data_channel_pdu_memory_size( 0 ) ];
        // the last LL PDU that was handed to the radio for transmission was the empty_ buffer
        bool                    next_empty_;
        bool                    empty_sequence_number_;

        static constexpr std::size_t  ll_header_size = 2;
        static constexpr std::uint8_t more_data_flag = 0x10;
        static constexpr std::uint8_t sn_flag        = 0x8;
        static constexpr std::uint8_t nesn_flag      = 0x4;
        static constexpr std::uint8_t ll_empty_id    = 0x01;


        const std::uint8_t* transmit_buffer() const
        {
            return &buffer_[ 0 ];
        }

        std::uint8_t* transmit_buffer()
        {
            return &buffer_[ 0 ];
        }

        const std::uint8_t* receive_buffer() const
        {
            return &buffer_[ TransmitSize ];
        }

        std::uint8_t* receive_buffer()
        {
            return &buffer_[ TransmitSize ];
        }

        write_buffer set_next_expected_sequence_number( read_buffer ) const;

        void acknowledge( bool sequence_number );
    };

    ///////////////////////////////////////
    // ll_data_buffer_sizes implementation
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    ll_data_buffer_sizes< TransmitSize, ReceiveSize, Layout >::ll_data_buffer_sizes()
        : current_ll_pdu_receive_size_(min_ll_pdu_size)
        , current_ll_pdu_transmit_size_(min_ll_pdu_size)
    {
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    std::size_t ll_data_buffer_sizes< TransmitSize, ReceiveSize, Layout >::current_ll_pdu_receive_size() const
    {
        return current_ll_pdu_receive_size_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    void ll_data_buffer_sizes< TransmitSize, ReceiveSize, Layout >::current_ll_pdu_receive_size( std::size_t max_size )
    {
        assert( max_size <= max_ll_pdu_receive_size() );
        assert( max_size >= min_ll_pdu_size );

        current_ll_pdu_receive_size_ = max_size;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    std::size_t ll_data_buffer_sizes< TransmitSize, ReceiveSize, Layout >::current_ll_pdu_transmit_size() const
    {
        return current_ll_pdu_transmit_size_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    void ll_data_buffer_sizes< TransmitSize, ReceiveSize, Layout >::current_ll_pdu_transmit_size( std::size_t max_size )
    {
        assert( max_size <= max_ll_pdu_receive_size() );
        assert( max_size >= min_ll_pdu_size );

        current_ll_pdu_transmit_size_ = max_size;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Layout  >
    void ll_data_buffer_sizes< TransmitSize, ReceiveSize, Layout >::reset_buffer_sizes()
    {
        current_ll_pdu_receive_size_   = min_ll_pdu_size;
        current_ll_pdu_transmit_size_  = min_ll_pdu_size;
    }

    ///////////////////////////////////////
    // ll_data_pdu_buffer implementation
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::ll_data_pdu_buffer()
        : receive_buffer_( receive_buffer() )
        , transmit_buffer_( transmit_buffer() )
    {
        layout_t::header( empty_, 0 );
        reset();
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::reset()
    {
        receive_buffer_.reset( receive_buffer() );
        transmit_buffer_.reset( transmit_buffer() );
        this->reset_buffer_sizes();

        sequence_number_ = false;
        next_expected_sequence_number_ = false;
        next_empty_      = false;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    std::uint8_t* ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::raw()
    {
        return &buffer_[ 0 ];
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_ll_transmit_buffer( std::size_t size )
    {
        typename Radio::lock_guard lock;

        return transmit_buffer_.alloc_front( transmit_buffer(), size + this->layout_overhead );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::commit_ll_transmit_buffer( read_buffer pdu )
    {
        static constexpr std::uint8_t header_rfu_mask = 0xe0;
        static_cast< void >( header_rfu_mask );

        // make sure, no NFU bits are set
        std::uint16_t header = layout_t::header( pdu );
        assert( ( header & header_rfu_mask ) == 0 );

        typename Radio::lock_guard lock;

        // add sequence number
        if ( sequence_number_ )
        {
            header |= sn_flag;
            layout_t::header( pdu, header );
        }

        sequence_number_ = !sequence_number_;

        transmit_buffer_.push_front( transmit_buffer(), pdu );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_l2cap_transmit_buffer( std::size_t size )
    {
        return allocate_ll_transmit_buffer( size );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::commit_l2cap_transmit_buffer( read_buffer pdu )
    {
        commit_ll_transmit_buffer( pdu );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::set_next_expected_sequence_number( read_buffer buf ) const
    {
        // insert the next expected sequence for every attempt to send the PDU, because it could be that
        // the slave is able to receive data, while the master is not able to.
        std::size_t header = layout_t::header( buf );

        header = next_expected_sequence_number_
            ? ( header | nesn_flag )
            : ( header & ~nesn_flag );

        layout_t::header( buf, header );

        return write_buffer( buf );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::next_transmit()
    {
        const read_buffer next = transmit_buffer_.next_end();

        if ( next_empty_ )
        {
            // if an empty buffer have to be resend, flag that there is more data
            if ( next.size )
            {
                const std::uint16_t header = layout_t::header( next ) | more_data_flag;
                layout_t::header( next, header );
            }

            return set_next_expected_sequence_number( read_buffer{ &empty_[ 0 ], sizeof( empty_ ) } );
        }
        else if ( next.size == 0 )
        {
            // we created an PDU, so it have to have a new sequnce number
            const std::uint16_t header = sequence_number_
                ? sn_flag + ll_empty_id
                :           ll_empty_id;

            layout_t::header( empty_, header );
            next_empty_ = true;

            // keep sequence number of empty in mind and increment sequence_number_
            empty_sequence_number_ = sequence_number_;
            sequence_number_ = !sequence_number_;

            return set_next_expected_sequence_number( read_buffer{ &empty_[ 0 ], sizeof( empty_ ) } );
        }

        if ( transmit_buffer_.more_than_one() )
            layout_t::header( next, layout_t::header( next ) | more_data_flag );

        return set_next_expected_sequence_number( next );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::acknowledge( bool nesn )
    {
        if ( next_empty_ )
        {
            if ( empty_sequence_number_ != nesn )
                next_empty_ = false;
        }
        else
        {
            const read_buffer next = transmit_buffer_.next_end();

            // the transmit buffer could be empty if we receive without sending prior. That happens during testing
            if ( next.empty() )
                return;

            const std::uint16_t header = layout_t::header( next );
            if ( static_cast< bool >( header & sn_flag ) != nesn )
            {
                transmit_buffer_.pop_end( transmit_buffer() );
                static_cast< Radio* >( this )->increment_transmit_packet_counter();
            }
        }
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_ll_transmit_buffer()
    {
        return allocate_ll_transmit_buffer( this->current_ll_pdu_transmit_size() );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::next_ll_l2cap_received() const
    {
        typename Radio::lock_guard lock;

        return write_buffer( receive_buffer_.next_end() );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    void ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::free_ll_l2cap_received()
    {
        typename Radio::lock_guard lock;

        receive_buffer_.pop_end( receive_buffer() );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    read_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::allocate_receive_buffer() const
    {
        const std::size_t raw_buffer_size = layout_t::data_channel_pdu_memory_size( this->current_ll_pdu_receive_size() - ll_header_size ) ;

        return receive_buffer_.alloc_front( const_cast< std::uint8_t* >( receive_buffer() ), raw_buffer_size );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename Radio >
    write_buffer ll_data_pdu_buffer< TransmitSize, ReceiveSize, Radio >::received( read_buffer pdu )
    {
        const std::uint16_t header = layout_t::header( pdu );

        // invalid LLID
        if ( ( header & 0x3 ) != 0 )
        {
            acknowledge( header & nesn_flag );

            // resent PDU?
            if ( static_cast< bool >( header & sn_flag ) == next_expected_sequence_number_ )
            {
                next_expected_sequence_number_ = !next_expected_sequence_number_;

                if ( ( header & 0xff00 ) != 0 )
                {
                    receive_buffer_.push_front( receive_buffer(), pdu );
                    static_cast< Radio* >( this )->increment_receive_packet_counter();
                }
            }
        }

        return next_transmit();
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
