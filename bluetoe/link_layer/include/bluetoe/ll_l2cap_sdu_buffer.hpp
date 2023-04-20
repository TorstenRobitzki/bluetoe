#ifndef BLUETOE_LINK_LAYER_L_L2CAP_SDU_BUFFER_HPP
#define BLUETOE_LINK_LAYER_L_L2CAP_SDU_BUFFER_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/bits.hpp>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief buffer responsible for fragment or defragment L2CAP SDUs
     *
     * If the L2CAP MTU size is 23, this class shall no generate any overhead as SDUs are directly
     * mapped to LL PDUs.
     */
    template < class BufferedRadio, std::size_t MTUSize >
    class ll_l2cap_sdu_buffer : public BufferedRadio
    {
    public:
        ll_l2cap_sdu_buffer();

        /**
         * @brief allocate a L2CAP buffer that is ready to be filled by the L2CAP layer
         *
         * If the request can not be fullfilled, the returned buffer will have a zero size.
         * After the buffer was filled by the L2CAP layer, commit_l2cap_transmit_buffer() must
         * be called to transmit the buffer.
         *
         * @param payload_size the payload size of the requested L2CAP buffer, must be >= 0 and <= MTUSize
         */
        read_buffer allocate_l2cap_transmit_buffer( std::size_t payload_size );

        /**
         * @brief allocates a LL buffer that is suitable for the given payload size
         *
         * If the request can not be fullfilled, the function will return an empty buffer.
         * After the buffer was filled by the link layer, commit_ll_transmit_buffer() must
         * be called to transmit the buffer.
         */
        read_buffer allocate_ll_transmit_buffer( std::size_t payload_size );

        /**
         * @brief schedules the given buffer to be send by the radio
         *
         * buffer must have been obtained by a call to allocate_l2cap_transmit_buffer()
         */
        void commit_l2cap_transmit_buffer( read_buffer buffer );

        /**
         * @brief schedules the given buffer to be send by the radio
         *
         * buffer must have been obtained by a call to allocate_ll_transmit_buffer()
         */
        void commit_ll_transmit_buffer( read_buffer buffer );

        /**
         * @brief returns the oldest link layer PDU or L2CAP SDU out of the receive buffer.
         *
         * If there is no oldest received PDU/SDU, an empty PDU will be returned (size == 0 ).
         * The returned PDU is not removed from the buffer. To remove the buffer after it is
         * not used any more, free_ll_l2cap_received() must be called.
         *
         * If there are pending outgoing PDUs there will be an attempt to write then to the
         * link layer.
         *
         * @pre buffer is in running mode
         *
         * @sa free_ll_l2cap_received()
         */
        write_buffer next_ll_l2cap_received();

        /**
         * @brief removes the oldest PDU from the receive buffer.
         * @pre next_ll_received() was called without free_ll_received() beeing called.
         * @pre buffer is in running mode
         */
        void free_ll_l2cap_received();

        /**
         * @brief radio layout assumed by the buffer
         */
        using layout = typename BufferedRadio::layout;

    private:
        static constexpr std::uint16_t  pdu_type_mask           = 0x0003;
        static constexpr std::uint16_t  pdu_type_link_layer     = 0x0003;
        static constexpr std::uint16_t  pdu_type_start          = 0x0002;
        static constexpr std::uint16_t  pdu_type_continuation   = 0x0001;

        static constexpr std::size_t    header_size             = BufferedRadio::header_size;
        static constexpr std::size_t    layout_overhead         = BufferedRadio::layout_overhead;
        static constexpr std::size_t    l2cap_header_size       = 4u;
        static constexpr std::size_t    overall_overhead        = header_size + layout_overhead + l2cap_header_size;
        static constexpr std::size_t    ll_overhead             = header_size + layout_overhead;

        void add_to_receive_buffer( const std::uint8_t*, const std::uint8_t* );
        void try_send_pdus();

        std::uint8_t    receive_buffer_[ MTUSize + overall_overhead ];
        std::uint16_t   receive_size_;
        std::size_t     receive_buffer_used_;

        // when transmitting L2CAP PDUs, we need room for at least the first LL header. Otherwise,
        // it would not be possible to transparently replace commit_l2cap_transmit_buffer()
        // transparently with commit_transmit_buffer() for the case that fragmentation is not
        // used.
        std::uint8_t    transmit_buffer_[ MTUSize + overall_overhead ];
        std::uint16_t   transmit_size_;
        std::size_t     transmit_buffer_used_;
    };

    /**
     * @brief specialisation for the minimum MTU size, which would not require any addition
     *        fragmentation / defragmentation
     */
    template < class BufferedRadio >
    class ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size > : public BufferedRadio
    {
    public:
        read_buffer allocate_l2cap_transmit_buffer( std::size_t size );
        read_buffer allocate_ll_transmit_buffer( std::size_t size );
        void commit_l2cap_transmit_buffer( read_buffer buffer );
        void commit_ll_transmit_buffer( read_buffer buffer );
        write_buffer next_ll_l2cap_received() const;
        void free_ll_l2cap_received();
    private:
        static constexpr std::size_t    header_size             = BufferedRadio::header_size;
        static constexpr std::size_t    layout_overhead         = BufferedRadio::layout_overhead;
        static constexpr std::size_t    l2cap_header_size       = 4u;
        static constexpr std::size_t    overall_overhead        = header_size + layout_overhead + l2cap_header_size;
        static constexpr std::size_t    ll_overhead             = header_size + layout_overhead;
    };

    // implementation
    template < class BufferedRadio, std::size_t MTUSize >
    ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::ll_l2cap_sdu_buffer()
        : receive_size_( 0 )
        , receive_buffer_used_( 0 )
        , transmit_size_( 0 )
        , transmit_buffer_used_( 0 )
    {
    }

    template < class BufferedRadio, std::size_t MTUSize >
    read_buffer ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::allocate_l2cap_transmit_buffer( std::size_t payload_size )
    {
        assert( payload_size <= MTUSize );

        if ( transmit_buffer_used_ != 0 ||  transmit_size_ != 0 )
            return { nullptr, 0 };

        return { transmit_buffer_, payload_size + overall_overhead };
    }

    template < class BufferedRadio, std::size_t MTUSize >
    read_buffer ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::allocate_ll_transmit_buffer( std::size_t payload_size )
    {
        try_send_pdus();

        return this->allocate_transmit_buffer( payload_size + ll_overhead );
    }

    template < class BufferedRadio, std::size_t MTUSize >
    void ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::commit_l2cap_transmit_buffer( read_buffer buffer )
    {
        const auto          body    = layout::body( buffer );
        const std::size_t   size    = bluetoe::details::read_16bit( body.first ) + overall_overhead;

        transmit_buffer_used_       = 0;
        transmit_size_              = size;

        try_send_pdus();
    }

    template < class BufferedRadio, std::size_t MTUSize >
    void ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::commit_ll_transmit_buffer( read_buffer buffer )
    {
        this->commit_transmit_buffer( buffer );
    }

    template < class BufferedRadio, std::size_t MTUSize >
    write_buffer ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::next_ll_l2cap_received()
    {
        try_send_pdus();

        // is there already a defragmented L2CAP SDU?
        if ( receive_buffer_used_ != 0 && receive_size_ == 0 )
            return { receive_buffer_, receive_buffer_used_ };

        for ( auto pdu = this->next_received(); pdu.size; pdu = this->next_received() )
        {
            const std::uint16_t header = layout::header( pdu );
            const std::uint16_t type   = header & pdu_type_mask;

            // link layer control PDUs are not fragmented by default
            if ( type == pdu_type_link_layer )
                return pdu;

            // l2cap message
            const auto          body        = layout::body( pdu );
            const std::size_t   body_size   = body.second - body.first;

            if ( type == pdu_type_start )
            {
                if ( body_size >= l2cap_header_size )
                {
                    const std::uint16_t l2cap_size  = bluetoe::details::read_16bit( body.first );

                    // short l2cap PDU that is not fragmented
                    if ( l2cap_size + l2cap_header_size == body_size )
                        return pdu;

                    if ( l2cap_size <= MTUSize )
                    {
                        receive_size_ = l2cap_size + overall_overhead;
                        add_to_receive_buffer( pdu.buffer, pdu.buffer + pdu.size );
                    }
                }
            }
            else
            {
                add_to_receive_buffer( body.first, body.second );
            }

            // consume LL PDU
            this->free_received();

            // is there now a defragmented L2CAP SDU?
            if ( receive_buffer_used_ != 0 && receive_size_ == 0 )
                return { receive_buffer_, receive_buffer_used_ };
        }

        return { nullptr, 0 };
    }

    template < class BufferedRadio, std::size_t MTUSize >
    void ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::add_to_receive_buffer( const std::uint8_t* begin, const std::uint8_t* end )
    {
        const std::size_t copy_size = std::min< std::size_t >( receive_size_, end - begin );

        std::copy( begin, end, &receive_buffer_[ receive_buffer_used_ ] );
        receive_buffer_used_ += copy_size;
        receive_size_ -= copy_size;
    }

    template < class BufferedRadio, std::size_t MTUSize >
    void ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::try_send_pdus()
    {
        while ( transmit_size_ )
        {
            const bool first_fragment   = transmit_buffer_used_ == 0;

            // for the first PDU, the header overhead is already taken into account. For all additonal fragments,
            // an additional header has to be allocated.
            const std::size_t overhead  = first_fragment ? 0 : ll_overhead;
            const auto buffer           = this->allocate_transmit_buffer( std::min( transmit_size_ + overhead, this->max_tx_size() ) );

            if ( buffer.size == 0 )
                return;

            if ( first_fragment )
            {
                // The first fragment contains the original LL header
                const auto copy_size = std::min< std::size_t >( buffer.size, transmit_size_ );

                std::copy( &transmit_buffer_[ 0 ], &transmit_buffer_[ copy_size ], buffer.buffer );
                layout::header( buffer, pdu_type_start | ( ( copy_size - ll_overhead ) << 8 ) );

                transmit_size_        -= copy_size;
                transmit_buffer_used_ += copy_size;
            }
            else
            {
                // for every additional fragment, an additional header has to be generated
                const auto body        = layout::body( buffer );
                const auto copy_size   = std::min< std::size_t >( std::distance( body.first, body.second ), transmit_size_ );

                std::copy( &transmit_buffer_[ transmit_buffer_used_ ], &transmit_buffer_[ transmit_buffer_used_+ copy_size ], body.first );
                layout::header( buffer, pdu_type_continuation | ( copy_size << 8 ) );

                transmit_size_        -= copy_size;
                transmit_buffer_used_ += copy_size;
            }

            this->commit_transmit_buffer( buffer );
        }

        transmit_buffer_used_ = 0;
    }

    template < class BufferedRadio, std::size_t MTUSize >
    void ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::free_ll_l2cap_received()
    {
        if (receive_buffer_used_)
        {
            receive_buffer_used_ = 0;
            receive_size_ = 0;
        }
        else
        {
            this->free_received();
        }
    }


    // implementation
    template < class BufferedRadio >
    read_buffer ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size >::allocate_l2cap_transmit_buffer( std::size_t size )
    {
        return this->allocate_transmit_buffer( size + overall_overhead );
    }

    template < class BufferedRadio >
    read_buffer ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size >::allocate_ll_transmit_buffer( std::size_t size )
    {
        return this->allocate_transmit_buffer( size + ll_overhead );
    }

    template < class BufferedRadio >
    void ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size >::commit_l2cap_transmit_buffer( read_buffer buffer )
    {
        return this->commit_transmit_buffer( buffer );
    }

    template < class BufferedRadio >
    void ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size >::commit_ll_transmit_buffer( read_buffer buffer )
    {
        return this->commit_transmit_buffer( buffer );
    }

    template < class BufferedRadio >
    write_buffer ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size >::next_ll_l2cap_received() const
    {
        return this->next_received();
    }

    template < class BufferedRadio >
    void ll_l2cap_sdu_buffer< BufferedRadio, bluetoe::details::default_att_mtu_size >::free_ll_l2cap_received()
    {
        return this->free_received();
    }
}
}

#endif
