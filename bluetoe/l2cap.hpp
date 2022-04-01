#ifndef BLUETOE_L2CAP_HPP
#define BLUETOE_L2CAP_HPP

#include <cstdint>
#include <cstddef>

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

        class l2cap_channel
        {
        public:
            static constexpr std::uint16_t channel_id = 42;
            static constexpr std::size_t   minimum_channel_mtu_size = bluetoe::details::default_att_mtu_size;
            static constexpr std::size_t   maximum_channel_mtu_size = bluetoe::details::default_att_mtu_size;

            template < typename ConnectionData >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );

            constexpr std::size_t channel_mtu_size() const;

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
         * void commit_l2cap_output_buffer( std::pair< std::size_t, std::uint8_t* > )
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
            bool handle_l2cap_input( const std::uint8_t* input, std::size_t in_size, ConnectionDetails& connection )
            {
                // just swallow input, if not resonable
                if ( in_size < header_size )
                    return true;

                const std::uint16_t size       = read_16bit( input );
                const std::uint16_t channel_id = read_16bit( input + 2 );

                if ( in_size != size + header_size )
                    return true;

                auto output = link_layer().allocate_l2cap_output_buffer( minimum_mtu_size + header_size );
                if ( output.first == 0 )
                    return false;

                std::size_t out_size = output.first - header_size;

                l2cap_input_handler< ConnectionDetails > handler(
                    this, channel_id, input + header_size, in_size - header_size,
                    output.second + header_size, out_size, connection );

                for_< Channels... >::template each< l2cap_input_handler< ConnectionDetails >& >( handler );

                if ( handler.handled && out_size )
                {
                    write_16bit( output.second, out_size );
                    write_16bit( output.second + 2, channel_id );
                    link_layer().commit_l2cap_output_buffer( { out_size + header_size, output.second } );
                }

                return true;
            }

            /**
             * @brief function to be called one every connection event, from the link layer
             *        to collect outstanding responses.
             */
            void transmit_pending_l2cap_output()
            {
                // transmit_notifications();
                // transmit_signaling_channel_output();
                // transmit_security_manager_channel_output();
            }

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
            struct l2cap_input_handler
            {
                l2cap_input_handler( l2cap* t, std::uint16_t ci, const std::uint8_t* i, std::size_t is, std::uint8_t* o, std::size_t& os, ConnectionDetails& c )
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
                std::size_t&                out_size;
                ConnectionDetails&          connection;
                bool                        handled;
            };

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer&>( *this );
            }

            static constexpr std::size_t header_size = 4u;
        };
    }
}

#if 0
    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_notifications()
    {
        // TODO: This should be a loop

        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_l2cap_transmit_buffer( connection_details_.negotiated_mtu() );

        if ( out_buffer.empty() )
            return;

        const auto notification = connection_details_.dequeue_indication_or_confirmation();

        if ( notification.first != connection_details_t::entry_type::empty )
        {
            std::size_t   out_size = out_buffer.size - all_header_size - layout_t::data_channel_pdu_memory_size( 0 );
            std::uint8_t* out_body = layout_t::body( out_buffer ).first;

            if ( notification.first == connection_details_t::entry_type::notification )
            {
                server_->notification_output(
                    &out_body[ l2cap_header_size ],
                    out_size,
                    connection_details_,
                    notification.second
                );
            }
            else
            {
                server_->indication_output(
                    &out_body[ l2cap_header_size ],
                    out_size,
                    connection_details_,
                    notification.second
                );

                // if no output is generate, confirm the indication, or we will wait for ever
                if ( out_size == 0 )
                    connection_details_.indication_confirmed();

            }

            if ( out_size )
            {
                fill< layout_t >( out_buffer, {
                    lld_data_pdu_code,
                    static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                    static_cast< std::uint8_t >( out_size & 0xff ),
                    static_cast< std::uint8_t >( ( out_size & 0xff00 ) >> 8 ),
                    static_cast< std::uint8_t >( l2cap_att_channel ),
                    static_cast< std::uint8_t >( l2cap_att_channel >> 8 ) } );

                this->commit_l2cap_transmit_buffer( out_buffer );
            }
        }
    }

            // transmit_notifications();

        };
   }
}
    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_signaling_channel_output()
    {
        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_l2cap_transmit_buffer( this->signaling_channel_mtu_size() );

        if ( out_buffer.empty() )
            return;

        std::size_t   out_size = out_buffer.size - all_header_size - layout_t::data_channel_pdu_memory_size( 0 );
        std::uint8_t* out_body = layout_t::body( out_buffer ).first;

        this->signaling_channel_output( &out_body[ l2cap_header_size ], out_size );

        if ( out_size )
        {
            fill< layout_t >( out_buffer, {
                lld_data_pdu_code,
                static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                static_cast< std::uint8_t >( out_size ),
                static_cast< std::uint8_t >( ( out_size & 0xff00 ) >> 8 ),
                static_cast< std::uint8_t >( l2cap_signaling_channel ),
                static_cast< std::uint8_t >( l2cap_signaling_channel >> 8 ) } );

            this->commit_l2cap_transmit_buffer( out_buffer );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_security_manager_channel_output()
    {
        const std::size_t mtu_size = this->security_manager_channel_mtu_size();
        if ( !mtu_size )
            return;

        if ( !this->security_manager_output_available( connection_details_ ) )
            return;

        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_l2cap_transmit_buffer( mtu_size );

        if ( out_buffer.empty() )
            return;

        std::size_t   out_size = out_buffer.size - all_header_size - layout_t::data_channel_pdu_memory_size( 0 );
        std::uint8_t* out_body = layout_t::body( out_buffer ).first;

        this->l2cap_output( &out_body[ l2cap_header_size ], out_size, connection_details_, *this );

        if ( out_size )
        {
            fill< layout_t >( out_buffer, {
                lld_data_pdu_code,
                static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                static_cast< std::uint8_t >( out_size ),
                static_cast< std::uint8_t >( ( out_size & 0xff00 ) >> 8 ),
                static_cast< std::uint8_t >( l2cap_sm_channel ),
                static_cast< std::uint8_t >( l2cap_sm_channel >> 8 ) } );

            this->commit_l2cap_transmit_buffer( out_buffer );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_l2cap( const write_buffer& input, const read_buffer& output )
    {
        const std::uint8_t* const input_body= layout_t::body( input ).first;

        const std::uint16_t l2cap_size      = read_16( &input_body[ 0 ] );
        const std::uint16_t l2cap_channel   = read_16( &input_body[ 2 ] );

        std::size_t   out_size   = output.size - l2cap_header_size - layout_t::data_channel_pdu_memory_size( 0 );
        std::uint8_t* out_body   = layout_t::body( output ).first;

        if ( l2cap_channel == l2cap_att_channel )
        {
            server_->l2cap_input( &input_body[ l2cap_header_size ], l2cap_size, &out_body[ l2cap_header_size ], out_size, connection_details_ );
        }
        else if ( l2cap_channel == l2cap_sm_channel )
        {
            static_cast< security_manager_t& >( *this ).l2cap_input( &input_body[ l2cap_header_size ], l2cap_size, &out_body[ l2cap_header_size ], out_size, connection_details_, *this );

            // in case the pairing status changed
            connection_details_.pairing_status( connection_details_.local_device_pairing_status() );
        }
        else if ( l2cap_channel == l2cap_signaling_channel )
        {
            this->signaling_channel_input(
                &input_body[ l2cap_header_size ], l2cap_size, &out_body[ l2cap_header_size ], out_size );
        }
        else
        {
            out_size = 0;
        }

        if ( out_size )
        {
            fill< layout_t >( output, {
                lld_data_pdu_code,
                static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                static_cast< std::uint8_t >( out_size ),
                0,
                static_cast< std::uint8_t >( l2cap_channel ),
                static_cast< std::uint8_t >( l2cap_channel >> 8 ) } );

            this->commit_l2cap_transmit_buffer( output );
        }

        return ll_result::go_ahead;
    }

        typedef notification_queue<
            typename Server::notification_priority::template numbers< typename Server::services >::type,
            typename Server::connection_data > notification_queue_t;

        typedef typename security_manager_t::template connection_data< notification_queue_t > connection_details_t;


#endif
#endif
