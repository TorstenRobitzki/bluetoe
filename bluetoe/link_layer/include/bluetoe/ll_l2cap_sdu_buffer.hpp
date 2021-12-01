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
         */
        read_buffer allocate_l2cap_transmit_buffer( std::size_t size );

        /**
         * @brief schedules the given buffer to be send by the radio
         *
         * buffer must have been obtained by a call to allocate_l2cap_transmit_buffer()
         */
        void commit_l2cap_transmit_buffer( read_buffer buffer );

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
        write_buffer next_ll_l2cap_received();

        /**
         * @brief removes the oldest PDU from the receive buffer.
         * @pre next_ll_received() was called without free_ll_received() beeing called.
         * @pre buffer is in running mode
         */
        void free_ll_l2cap_received();

    private:
        using layout = typename BufferedRadio::layout;

        static constexpr std::uint16_t  pdu_type_mask        = 0x0003;
        static constexpr std::uint16_t  pdu_type_link_layer  = 0x0003;
        static constexpr std::uint16_t  pdu_type_start       = 0x0002;
        static constexpr std::uint16_t  pdu_type_continuation= 0x0001;

        static constexpr std::size_t    header_size     = BufferedRadio::header_size;
        static constexpr std::size_t    layout_overhead = BufferedRadio::layout_overhead;
        static constexpr std::size_t    l2cap_header_size = 4u;
        static constexpr std::size_t    overall_overhead = header_size + layout_overhead + l2cap_header_size;

        void add_to_receive_buffer( const std::uint8_t*, const std::uint8_t* );

        std::uint8_t    receive_buffer_[ MTUSize + overall_overhead ];
        std::uint16_t   receive_size_;
        std::size_t     receive_buffer_used_;
        std::uint8_t    transmit_buffer_[ MTUSize + overall_overhead ];
    };

    template < class BufferedRadio >
    class ll_l2cap_sdu_buffer< BufferedRadio, details::default_att_mtu_size > : public BufferedRadio
    {
    public:
        /**
         * @brief allocate a L2CAP buffer that is ready to be filled by the L2CAP layer
         *
         * If the request can not be fullfilled, the returned buffer will have a zero size.
         * After the buffer was filled by the L2CAP layer, commit_l2cap_transmit_buffer() must
         * be called to transmit the buffer.
         */
        read_buffer allocate_l2cap_transmit_buffer( std::size_t size );

        /**
         * @brief schedules the given buffer to be send by the radio
         *
         * buffer must have been obtained by a call to allocate_l2cap_transmit_buffer()
         */
        void commit_l2cap_transmit_buffer( read_buffer buffer );

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
    };

    // implementation
    template < class BufferedRadio, std::size_t MTUSize >
    ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::ll_l2cap_sdu_buffer()
        : receive_size_( 0 )
        , receive_buffer_used_( 0 )
    {
    }

    template < class BufferedRadio, std::size_t MTUSize >
    read_buffer ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::allocate_l2cap_transmit_buffer( std::size_t /*size*/ )
    {
        return {0,0};
    }

    template < class BufferedRadio, std::size_t MTUSize >
    void ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::commit_l2cap_transmit_buffer( read_buffer buffer )
    {
        (void)buffer;
    }

    template < class BufferedRadio, std::size_t MTUSize >
    write_buffer ll_l2cap_sdu_buffer< BufferedRadio, MTUSize >::next_ll_l2cap_received()
    {
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
                    const std::uint16_t l2cap_size  = details::read_16bit( body.first );

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
    read_buffer ll_l2cap_sdu_buffer< BufferedRadio, details::default_att_mtu_size >::allocate_l2cap_transmit_buffer( std::size_t size )
    {
        return this->allocate_transmit_buffer( size );
    }

    template < class BufferedRadio >
    void ll_l2cap_sdu_buffer< BufferedRadio, details::default_att_mtu_size >::commit_l2cap_transmit_buffer( read_buffer buffer )
    {
        return this->commit_transmit_buffer( buffer );
    }

    template < class BufferedRadio >
    write_buffer ll_l2cap_sdu_buffer< BufferedRadio, details::default_att_mtu_size >::next_ll_l2cap_received() const
    {
        return this->next_received();
    }

    template < class BufferedRadio >
    void ll_l2cap_sdu_buffer< BufferedRadio, details::default_att_mtu_size >::free_ll_l2cap_received()
    {
        return this->free_received();
    }
}
}

#endif
