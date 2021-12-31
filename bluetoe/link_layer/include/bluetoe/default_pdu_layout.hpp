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
        /**
         * @brief fixed 16 bit size of a LL PDU
         */
        static constexpr std::size_t header_size = sizeof( std::uint16_t );

        using details::layout_base< default_pdu_layout >::header;

        /**
         * @brief retrieves the 16 bit LL header from a PDU.
         */
        static std::uint16_t header( const std::uint8_t* pdu )
        {
            return ::bluetoe::details::read_16bit( pdu );
        }

        /**
         * @brief sets the 16 but LL header for the pdu
         */
        static void header( std::uint8_t* pdu, std::uint16_t header_value )
        {
            ::bluetoe::details::write_16bit( pdu, header_value );
        }

        /**
         * @brief returns a begin and end pointer to the body of a PDU
         *
         * That pair can be used to modify the body
         */
        static std::pair< std::uint8_t*, std::uint8_t* > body( const read_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size ], &pdu.buffer[ pdu.size ] };
        }

        /**
         * @brief returns a begin and end pointer to the body of a PDU
         */
        static std::pair< const std::uint8_t*, const std::uint8_t* > body( const write_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size ], &pdu.buffer[ pdu.size ] };
        }

        /**
         * @brief returns the overall, required buffer size for a PDU with the given
         *        payload_size.
         */
        static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
        {
            return header_size + payload_size;
        }
    };

    /**
     * @brief type to associate a radio implementation with the corresponding layout
     *
     * Must be specialized by radio implementations that do not use the default_pdu_layout.
     */
    template < typename Radio >
    struct pdu_layout_by_radio {
        /**
         * @brief the layout to be applied when Radio is used
         */
        using pdu_layout = default_pdu_layout;
    };

}
}

#endif
