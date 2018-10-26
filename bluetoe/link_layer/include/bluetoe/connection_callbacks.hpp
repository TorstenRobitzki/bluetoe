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
     *          const ConnectionData&                            connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_changed(
     *          const bluetoe::link_layer::connection_details&  details,
     *          const ConnectionData&                           connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_closed( const ConnectionData& connection );
     */
    template < typename T, T& Obj >
    struct connection_callbacks {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::connection_callbacks_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < class Server >
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
            template < class Radio >
            void connection_established(
                const connection_details&               details,
                const typename Server::connection_data& connection,
                Radio&                                  r )
            {
                event_type_ = established;
                connection_ = &connection;
                details_    = details;

                r.wake_up();
            }

            template < class Radio >
            void connection_changed( const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection, Radio& r )
            {
                event_type_ = changed;
                connection_ = &connection;
                details_    = details;

                r.wake_up();
            }

            template < class Radio >
            void connection_closed( const typename Server::connection_data& connection, Radio& r )
            {
                event_type_ = closed;
                connection_ = &connection;

                r.wake_up();
            }

            void handle_connection_events() {
                if ( event_type_ == established )
                {
                    call_ll_connection_established< T >( Obj, details_, addresses_, *connection_ );
                }
                else if ( event_type_ == changed )
                {
                    call_ll_connection_changed< T >( Obj, details_, *connection_ );
                }
                else if ( event_type_ == closed )
                {
                    call_ll_connection_closed< T >( Obj, *connection_ );
                }

                event_type_ = none;
                connection_ = nullptr;
            }

        private:
            enum {
                none,
                established,
                changed,
                closed
            } event_type_;

            const typename Server::connection_data*     connection_;
            connection_details                          details_;
            connection_addresses                        addresses_;

            template < typename TT >
            auto call_ll_connection_established(
                TT&                                     obj,
                const connection_details&               details,
                const connection_addresses&             addr,
                const typename Server::connection_data& connection )
                    -> decltype(&TT::template ll_connection_established< typename Server::connection_data >)
            {
                obj.ll_connection_established( details, addr, connection );

                return 0;
            }

            template < typename TT >
            auto call_ll_connection_changed( TT& obj, const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection )
                -> decltype(&TT::template ll_connection_changed< typename Server::connection_data >)
            {
                obj.ll_connection_changed( details, connection );

                return 0;
            }

            template < typename TT >
            auto call_ll_connection_closed( TT& obj, const typename Server::connection_data& connection )
                -> decltype(&TT::template ll_connection_closed< typename Server::connection_data >)
            {
                obj.ll_connection_closed( connection );

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
        };

        /** @endcond */
    };

    namespace details {
        struct no_connection_callbacks {
            struct meta_type :
                details::connection_callbacks_meta_type,
                details::valid_link_layer_option_meta_type {};

            template < class Server >
            struct impl {
                void connection_request( const connection_addresses& ) {}

                template < class Radio >
                void connection_established(
                    const connection_details&,
                    const typename Server::connection_data&,
                    Radio& ) {}

                template < class Radio >
                void connection_changed(  const bluetoe::link_layer::connection_details&, const typename Server::connection_data&, Radio& ) {}

                template < class Radio >
                void connection_closed( const typename Server::connection_data&, Radio& ) {}

                void handle_connection_events() {}
            };
        };
    }

}
}

#endif
