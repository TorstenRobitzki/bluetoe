#ifndef BLUETOE_L2CAP_HPP
#define BLUETOE_L2CAP_HPP

#include <cstdint>
#include <cstddef>
#include <cassert>

#include <bluetoe/codes.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/bits.hpp>

/*
 * Design decisions:
 * - Link layer should not know nothing about l2cap fragmentation / channels and so on
 * - L2CAP layer should not know anything about LL PDU layouts
 * - link_layer::read_buffer / link_layer::read_buffer should be assigned to LL PDUs and their
 *   layout and thus, not be used in the l2cap layer.
 * - For now, the fragmentation, defragmentation is still done in the ll_l2cap_sdu_buffer
 *   of the link_layer, but should be moved into the l2cap<> class.
 */
namespace bluetoe {

namespace details {

    /**
     * @brief this class documents the requirements of a single l2cap channel to satisfy
     *        the requirements of the l2cap<> class.
     */
    class l2cap_channel
    {
    public:
        static constexpr std::uint16_t channel_id = 42;
        static constexpr std::size_t   minimum_channel_mtu_size = bluetoe::details::default_att_mtu_size;
        static constexpr std::size_t   maximum_channel_mtu_size = bluetoe::details::default_att_mtu_size;

        template < typename ConnectionData >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );

        template < typename ConnectionData >
        void l2cap_output( std::uint8_t* output, std::size_t& out_size, ConnectionData& );

        template < class PreviousData >
        using channel_data_t = PreviousData;
    };

    template < typename CurrentMaximum, typename Channel >
    struct maximum_min_channel_mtu_size
    {
        using type =
            typename select_type<
                ( CurrentMaximum::value > Channel::minimum_channel_mtu_size ),
                CurrentMaximum,
                std::integral_constant< std::size_t, Channel::minimum_channel_mtu_size >
            >::type;
    };

    template < typename CurrentMaximum, typename Channel >
    struct maximum_max_channel_mtu_size
    {
        using type =
            typename select_type<
                ( CurrentMaximum::value > Channel::maximum_channel_mtu_size ),
                CurrentMaximum,
                std::integral_constant< std::size_t, Channel::maximum_channel_mtu_size >
            >::type;
    };

    template < typename ComposedData, typename Channel >
    struct add_channel_data
    {
        using type = typename Channel::template channel_data_t< ComposedData >;
    };

    static constexpr std::size_t l2cap_layer_header_size = 4u;

    /**
     * @brief l2cap layer, as list of l2cap channels
     *
     * The interface between link_layer and l2cap is designed, so that
     * both parts could be run in different CPU contexts. This basically means,
     * that the reponse to an incomming PDU must be able to be defered. That's
     * why the functions do not directly return the result of the L2CAP request,
     * but use the output parameters to allocate buffers and commit that buffers.
     *
     * LinkLayer derives from l2cap<> and provides following functions:
     *
     * - std::pair< std::size_t, std::uint8_t* > allocate_l2cap_output_buffer( std::size_t )
     *
     *   This function is used to allocate outgoing capacity. If the function can not provide
     *   the requested size, it has to provide { 0, nullptr }.
     *
     * - void commit_l2cap_output_buffer( std::pair< std::size_t, std::uint8_t* > )
     *
     *   This function is used to commit the allocated output buffer.
     */
    template < class LinkLayer, class ChannelData, class ... Channels >
    class l2cap : public derive_from< std::tuple< Channels... > >
    {
    public:
        /**
         * @ret true, if the passed input was consumed
         */
        template < class ConnectionDetails >
        bool handle_l2cap_input( const std::uint8_t* input, std::size_t in_size, ConnectionDetails& connection );

        /**
         * @brief function to be called one every connection event, from the link layer
         *        to collect outstanding responses.
         */
        template < class ConnectionDetails >
        void transmit_pending_l2cap_output( ConnectionDetails& connection );

        /**
         * @brief the minimum MTU size, that is required by all L2CAP channels
         *
         * This is the size without the 4 byte L2CAP header.
         */
        static constexpr std::size_t minimum_mtu_size = fold<
            std::tuple< Channels... >,
            maximum_min_channel_mtu_size,
            std::integral_constant< std::size_t, 0 >
        >::type::value;

        static constexpr std::size_t maximum_mtu_size = fold<
            std::tuple< Channels... >,
            maximum_max_channel_mtu_size,
            std::integral_constant< std::size_t, 0 >
        >::type::value;

        static_assert( maximum_mtu_size >= minimum_mtu_size, "maximum_mtu_size have to be greater than minimum_mtu_size" );

        using connection_data_t = typename fold<
            std::tuple< Channels... >,
            add_channel_data,
            ChannelData
        >::type;

    private:
        template < class ConnectionDetails >
        bool transmit_single_pending_l2cap_output( ConnectionDetails& connection );

        template < class ConnectionDetails >
        struct l2cap_input_handler
        {
            l2cap_input_handler( l2cap* t, std::uint16_t ci, const std::uint8_t* i, std::size_t is, std::uint8_t* o, std::size_t os, ConnectionDetails& c )
                : that( t )
                , channel_id( ci )
                , input( i )
                , in_size( is )
                , output( o )
                , out_size( os )
                , connection( c )
                , handled( false )
            {
            }

            template< typename Channel >
            void each()
            {
                if ( channel_id == Channel::channel_id )
                {
                    static_cast< Channel& >( *that ).l2cap_input( input, in_size, output, out_size, connection );
                    handled = true;
                }
            }

            l2cap*                      that;
            std::uint16_t               channel_id;
            const std::uint8_t*         input;
            std::size_t                 in_size;
            std::uint8_t*               output;
            std::size_t                 out_size;
            ConnectionDetails&          connection;
            bool                        handled;
        };

        template < typename ConnectionDetails >
        class l2cap_output_handler
        {
        public:
            l2cap_output_handler( l2cap* t, std::uint8_t* out, std::size_t s, ConnectionDetails& c )
                : that( t )
                , output( out )
                , size( s )
                , out_size( 0 )
                , connection( c )
            {
            }

            template< typename Channel >
            void each()
            {
                if ( out_size == 0 )
                {
                    out_size = size;
                    static_cast< Channel& >( *that ).l2cap_output( output, out_size, connection );
                    channel_id = Channel::channel_id;
                }
            }

            l2cap*              that;
            std::uint8_t*       output;
            std::size_t         size;
            std::size_t         out_size;
            std::uint16_t       channel_id;
            ConnectionDetails&  connection;
        };

        LinkLayer& link_layer()
        {
            return static_cast< LinkLayer&>( *this );
        }
    };


    // implemenation
    template < class LinkLayer, class ChannelData, class ... Channels >
    template < class ConnectionDetails >
    bool l2cap< LinkLayer, ChannelData, Channels... >::handle_l2cap_input( const std::uint8_t* input, std::size_t in_size, ConnectionDetails& connection )
    {
        // just swallow input, if not resonable
        if ( in_size < l2cap_layer_header_size )
            return true;

        const std::uint16_t size       = read_16bit( input );
        const std::uint16_t channel_id = read_16bit( input + 2 );

        if ( in_size != size + l2cap_layer_header_size )
            return true;

        auto output = link_layer().allocate_l2cap_output_buffer( maximum_mtu_size );
        if ( output.first == 0 )
            return false;

        assert( output.second );

        l2cap_input_handler< ConnectionDetails > handler(
            this, channel_id, input + l2cap_layer_header_size, in_size - l2cap_layer_header_size,
            output.second + l2cap_layer_header_size, maximum_mtu_size, connection );

        for_< Channels... >::template each< l2cap_input_handler< ConnectionDetails >& >( handler );

        if ( handler.handled && handler.out_size )
        {
            write_16bit( output.second,  handler.out_size );
            write_16bit( output.second + 2, channel_id );
            link_layer().commit_l2cap_output_buffer( {  handler.out_size + l2cap_layer_header_size, output.second } );
        }

        return true;
    }

    template < class LinkLayer, class ChannelData, class ... Channels >
    template < class ConnectionDetails >
    void l2cap< LinkLayer, ChannelData, Channels... >::transmit_pending_l2cap_output( ConnectionDetails& connection )
    {
        for ( bool cont = transmit_single_pending_l2cap_output( connection ); cont;
                cont = transmit_single_pending_l2cap_output( connection ) )
            ;
    }

    template < class LinkLayer, class ChannelData, class ... Channels >
    template < class ConnectionDetails >
    bool l2cap< LinkLayer, ChannelData, Channels... >::transmit_single_pending_l2cap_output( ConnectionDetails& connection )
    {
        auto output = link_layer().allocate_l2cap_output_buffer( maximum_mtu_size );
        if ( output.first == 0 )
            return false;

        assert( output.second );

        l2cap_output_handler< ConnectionDetails > handler(
            this, output.second + l2cap_layer_header_size, output.first - l2cap_layer_header_size, connection );

        for_< Channels... >::template each< l2cap_output_handler< ConnectionDetails >& >( handler );

        if ( handler.out_size )
        {
            write_16bit( output.second, handler.out_size );
            write_16bit( output.second + 2, handler.channel_id );
            link_layer().commit_l2cap_output_buffer( { handler.out_size + l2cap_layer_header_size, output.second } );
        }

        return handler.out_size;
    }
}
}

#endif
