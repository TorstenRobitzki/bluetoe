#ifndef BLUETOE_LINK_LAYER_CONNECTION_CALLBACKS_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_CALLBACKS_HPP

#include <bluetoe/meta_types.hpp>

namespace bluetoe {
namespace link_layer {
    class connection_details;

    namespace details {
        struct connection_callbacks_meta_type {};
    }

    /**
     * @brief provides a type and an instance to call connection related callbacks on.
     *
     * The parameter T have to be a class type with following optional none static member
     * functions:
     *
     * template < typename ConnectionData >
     * void ll_connection_established(
     *          const bluetoe::link_layer::connection_details&   details,
     *          const bluetoe::link_layer::connection_addresses& addresses,
     *                ConnectionData&                            connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_changed(
     *          const bluetoe::link_layer::connection_details&  details,
     *                ConnectionData&                           connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_closed( ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_version( std::uint8_t version, std::uint16_t company, std::uint16_t subversion, const ConnectionData& connection );
     *
     */
    template < typename T, T& Obj >
    struct connection_callbacks {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::connection_callbacks_meta_type,
            details::valid_link_layer_option_meta_type {};

        // TODO No need for impl-indirection anymore
        class impl {
        public:
            impl()
                : event_type_( none )
                , connection_( nullptr )
            {
            }

            void connection_request( const connection_addresses& addresses )
            {
                addresses_  = addresses;
            }

            // this functions are called from the interrupt handlers of the scheduled radio and just store the informations that
            // are provided.
            template < class Connection, class Radio >
            void connection_established(
                const connection_details&   details,
                Connection&                 connection,
                Radio&                      r )
            {
                event_type_ = established;
                connection_ = &connection;
                details_    = details;

                r.wake_up();
            }

            template < class Connection, class Radio >
            void connection_changed( const bluetoe::link_layer::connection_details& details, Connection& connection, Radio& r )
            {
                event_type_ = changed;
                connection_ = &connection;
                details_    = details;

                r.wake_up();
            }

            template < class Connection, class Radio >
            void connection_closed( Connection& connection, Radio& r )
            {
                event_type_ = closed;
                connection_ = &connection;

                r.wake_up();
            }

            template < class Connection, class Radio >
            void version_indication_received( const std::uint8_t* details, Connection& connection, Radio& r )
            {
                event_type_ = version;
                connection_ = &connection;
                std::copy( details, details + version_ind_size, &raw_details_[ 0 ] );

                r.wake_up();
            }

            template < class LinkLayer >
            void handle_connection_events() {
                if ( event_type_ == established )
                {
                    call_ll_connection_established< T >( Obj, details_, addresses_, connection_data< LinkLayer >() );
                }
                else if ( event_type_ == changed )
                {
                    call_ll_connection_changed< T >( Obj, details_, connection_data< LinkLayer >() );
                }
                else if ( event_type_ == closed )
                {
                    call_ll_connection_closed< T >( Obj, connection_data< LinkLayer >() );
                }
                else if ( event_type_ == version )
                {
                    call_ll_version< T >( Obj, connection_data< LinkLayer >() );
                }

                event_type_ = none;
                connection_ = nullptr;
            }

        private:
            enum {
                none,
                established,
                changed,
                closed,
                version
            } event_type_;

            void*                   connection_;
            connection_details      details_;
            connection_addresses    addresses_;

            static constexpr std::size_t version_ind_size = 5u;
            std::uint8_t raw_details_[ version_ind_size ];

            template < typename LinkLayer >
            typename LinkLayer::connection_data_t& connection_data()
            {
                assert( connection_ );
                return *static_cast< typename LinkLayer::connection_data_t* >( connection_ );
            }

            template < typename TT, typename Connection >
            auto call_ll_connection_established(
                TT&                                     obj,
                const connection_details&               details,
                const connection_addresses&             addr,
                Connection&                             connection )
                    -> decltype(&TT::template ll_connection_established< Connection >)
            {
                obj.ll_connection_established( details, addr, connection );

                return 0;
            }

            template < typename TT, typename Connection >
            auto call_ll_connection_changed( TT& obj, const bluetoe::link_layer::connection_details& details, Connection& connection )
                -> decltype(&TT::template ll_connection_changed< Connection >)
            {
                obj.ll_connection_changed( details, connection );

                return 0;
            }

            template < typename TT, typename Connection >
            auto call_ll_connection_closed( TT& obj, Connection& connection )
                -> decltype(&TT::template ll_connection_closed< Connection >)
            {
                obj.ll_connection_closed( connection );

                return 0;
            }

            template < typename TT, typename Connection >
            auto call_ll_version( TT& obj, Connection& connection )
                -> decltype(&TT::template ll_version< Connection >)
            {
                obj.ll_version(
                    raw_details_[ 0 ],
                    bluetoe::details::read_16bit( &raw_details_[ 1 ] ),
                    bluetoe::details::read_16bit( &raw_details_[ 3 ] ),
                    connection );

                return 0;
            }

            template < typename TT >
            void call_ll_connection_established( ... )
            {
            }

            template < typename TT >
            void call_ll_connection_changed( ... )
            {
            }

            template < typename TT >
            void call_ll_connection_closed( ... )
            {
            }

            template < typename TT >
            void call_ll_version( ... )
            {
            }
        };

        /** @endcond */
    };

    namespace details {
        struct no_connection_callbacks {
            struct meta_type :
                details::connection_callbacks_meta_type,
                details::valid_link_layer_option_meta_type {};

            struct impl {
                void connection_request( const connection_addresses& ) {}

                template < class Connection, class Radio >
                void connection_established(
                    const connection_details&,
                    Connection&,
                    Radio& ) {}

                template < class Connection, class Radio >
                void connection_changed( const bluetoe::link_layer::connection_details&, Connection&, Radio& ) {}

                template < class Connection, class Radio >
                void connection_closed( Connection&, Radio& ) {}

                template < class LinkLayer >
                void handle_connection_events() {}

                template < class Connection, class Radio >
                void version_indication_received( const std::uint8_t*, Connection&, Radio& ) {}
            };
        };
    }

}
}

#endif
