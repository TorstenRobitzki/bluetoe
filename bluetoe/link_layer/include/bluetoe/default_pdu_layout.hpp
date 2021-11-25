#ifndef BLUETOE_LINK_LAYER_DEFAULT_PDU_LAYOUT_HPP
#define BLUETOE_LINK_LAYER_DEFAULT_PDU_LAYOUT_HPP

#include <bluetoe/bits.hpp>
#include <bluetoe/buffer.hpp>

namespace bluetoe {
namespace link_layer {

    namespace details {
        // usefull overloads
        template < class Base >
        struct layout_base
        {
            static std::uint16_t header( const read_buffer& pdu )
            {
                assert( pdu.size >= Base::data_channel_pdu_memory_size( 0 ) );

                return Base::header( pdu.buffer );
            }

            static std::uint16_t header( const write_buffer& pdu )
            {
                assert( pdu.size >= Base::data_channel_pdu_memory_size( 0 ) );

                return Base::header( pdu.buffer );
            }

            static void header( const read_buffer& pdu, std::uint16_t header_value )
            {
                assert( pdu.size >= Base::data_channel_pdu_memory_size( 0 ) );

                Base::header( pdu.buffer, header_value );
            }
        };
    }
    /**
     * @brief implements a PDU layout, where in memory and over the air layout are equal.
     *
     * The type is ment to be used as member of a scheduled_radio implementation.
     */
    struct default_pdu_layout : details::layout_base< default_pdu_layout >{
        static constexpr std::size_t header_size = sizeof( std::uint16_t );

        using details::layout_base< default_pdu_layout >::header;

        static std::uint16_t header( const std::uint8_t* pdu )
        {
            return ::bluetoe::details::read_16bit( pdu );
        }

        static void header( std::uint8_t* pdu, std::uint16_t header_value )
        {
            ::bluetoe::details::write_16bit( pdu, header_value );
        }

        static std::pair< std::uint8_t*, std::uint8_t* > body( const read_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size ], &pdu.buffer[ pdu.size ] };
        }

        static std::pair< const std::uint8_t*, const std::uint8_t* > body( const write_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size ], &pdu.buffer[ pdu.size ] };
        }

        /**
         * @brief calculates the overall PDU size from the payload size
         */
        static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
        {
            return header_size + payload_size;
        }
    };

    /**
     * @brief this template is ment to be specialized for a given Radio to define the layout
     *        used by that radio.
     *
     * Without specialization, default_pdu_layout is applied for a Radio.
     */
    template < typename Radio >
    struct pdu_layout_by_radio {
        using pdu_layout = default_pdu_layout;
    };

}
}

#endif
