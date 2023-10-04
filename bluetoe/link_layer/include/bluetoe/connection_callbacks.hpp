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
     * void ll_connection_requested(
     *          const bluetoe::link_layer::connection_details&   details,
     *          const bluetoe::link_layer::connection_addresses& addresses,
     *                ConnectionData&                            connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_attempt_timeout(
     *                ConnectionData&                            connection );
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
     * void ll_connection_closed( std::uint8_t reason, ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_version( std::uint8_t version, std::uint16_t company, std::uint16_t subversion, const ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_rejected( std::uint8_t error_code, const ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_unknown( std::uint8_t unknown_type, const ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_remote_features( std::uint8_t remote_features[ 8 ], const ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_phy_updated(
     *  bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmit_encoding,
     *  bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receive_encoding,
     *  const ConnectionData& connection );
     *
     * ll_connection_requested() will be called, as soon, as a connection request is received.
     * ll_connection_established() is called, after the first connection event actually toke place.
     *
     * ll_connection_attempt_timeout() will be called, when the connection was reported as beeing requested
     * (ll_connection_requested()) but not established (ll_connection_established()) and then timed out, because
     * not a single connection event happend.
     */
    template < typename T, T& Obj >
    struct connection_callbacks {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::connection_callbacks_meta_type,
            details::valid_link_layer_option_meta_type {};

        connection_callbacks()
            : event_type_( none )
            , connection_( nullptr )
        {
        }

        void connection_request( const connection_addresses& addresses )
        {
            addresses_  = addresses;
        }

        template < class Connection, class Radio >
        void connection_requested(
            const connection_details&   details,
            Connection&                 connection,
            Radio&                      r )
        {
            event_type_ = requested;
            connection_ = &connection;
            details_    = details;

            r.wake_up();
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
        void connection_attempt_timeout( Connection& connection, Radio& r )
        {
            event_type_ = attempt_timeout;
            connection_ = &connection;

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
        void connection_closed( std::uint8_t reason, Connection& connection, Radio& r )
        {
            event_type_ = closed;
            connection_ = &connection;
            raw_details_[ 0 ] = reason;

            r.wake_up();
        }

        template < class Connection, class Radio >
        void procedure_rejected( std::uint8_t error_code, Connection& connection, Radio& r )
        {
            event_type_ = rejected;
            connection_ = &connection;
            raw_details_[ 0 ] = error_code;

            r.wake_up();
        }

        template < class Connection, class Radio >
        void procedure_unknown( std::uint8_t error_code, Connection& connection, Radio& r )
        {
            event_type_ = unknown;
            connection_ = &connection;
            raw_details_[ 0 ] = error_code;

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

        template < class Connection, class Radio >
        void remote_features_received( const std::uint8_t rf[ 8 ], Connection& connection, Radio& r )
        {
            event_type_ = remote_features;
            connection_ = &connection;
            std::copy( rf, rf + feature_field_size, &raw_details_[ 0 ] );

            r.wake_up();
        }

        template < class Connection, class Radio >
        void phy_update( std::uint8_t phy_c_to_p, std::uint8_t phy_p_to_c, Connection& connection, Radio& r )
        {
            event_type_ = update_phy;
            connection_ = &connection;
            raw_details_[ 0 ] = phy_c_to_p;
            raw_details_[ 1 ] = phy_p_to_c;

            r.wake_up();
        }

        template < class LinkLayer >
        void handle_connection_events() {
            if ( event_type_ == requested )
            {
                call_ll_connection_requested< T >( Obj, details_, addresses_, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == attempt_timeout )
            {
                call_ll_connection_attempt_timeout< T >( Obj, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == established )
            {
                call_ll_connection_established< T >( Obj, details_, addresses_, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == changed )
            {
                call_ll_connection_changed< T >( Obj, details_, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == closed )
            {
                call_ll_connection_closed< T >( Obj, raw_details_[ 0 ], connection_data< LinkLayer >() );
            }
            else if ( event_type_ == version )
            {
                call_ll_version< T >( Obj, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == rejected )
            {
                call_ll_rejected< T >( Obj, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == unknown )
            {
                call_ll_unknown< T >( Obj, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == remote_features )
            {
                call_ll_remote_features< T >( Obj, connection_data< LinkLayer >() );
            }
            else if ( event_type_ == update_phy )
            {
                call_ll_phy_updated< T >( Obj, connection_data< LinkLayer >() );
            }

            event_type_ = none;
            connection_ = nullptr;
        }

    private:
        enum {
            none,
            requested,
            attempt_timeout,
            established,
            changed,
            closed,
            version,
            rejected,
            unknown,
            remote_features,
            update_phy
        } event_type_;

        void*                   connection_;
        connection_details      details_;
        connection_addresses    addresses_;

        static constexpr std::size_t version_ind_size = 5u;
        static constexpr std::size_t feature_field_size = 8u;

        std::uint8_t raw_details_[ feature_field_size ];

        template < typename LinkLayer >
        typename LinkLayer::connection_data_t& connection_data()
        {
            assert( connection_ );
            return *static_cast< typename LinkLayer::connection_data_t* >( connection_ );
        }

        template < typename TT, typename Connection >
        auto call_ll_connection_requested(
            TT&                                     obj,
            const connection_details&               details,
            const connection_addresses&             addr,
            Connection&                             connection )
                -> decltype(&TT::template ll_connection_requested< Connection >)
        {
            obj.ll_connection_requested( details, addr, connection );

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_connection_attempt_timeout( TT& obj, Connection& connection  )
            -> decltype(&TT::template ll_connection_attempt_timeout< Connection >)
        {
            obj.ll_connection_attempt_timeout( connection );

            return nullptr;
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

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_connection_changed( TT& obj, const bluetoe::link_layer::connection_details& details, Connection& connection )
            -> decltype(&TT::template ll_connection_changed< Connection >)
        {
            obj.ll_connection_changed( details, connection );

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_connection_closed( TT& obj, std::uint8_t reason, Connection& connection )
            -> decltype(&TT::template ll_connection_closed< Connection >)
        {
            obj.ll_connection_closed( reason, connection );

            return nullptr;
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

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_rejected( TT& obj, Connection& connection )
            -> decltype(&TT::template ll_rejected< Connection >)
        {
            obj.ll_rejected(
                raw_details_[ 0 ],
                connection );

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_unknown( TT& obj, Connection& connection )
            -> decltype(&TT::template ll_unknown< Connection >)
        {
            obj.ll_unknown(
                raw_details_[ 0 ],
                connection );

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_remote_features( TT& obj, Connection& connection )
            -> decltype(&TT::template ll_remote_features< Connection >)
        {
            obj.ll_remote_features(
                &raw_details_[ 0 ],
                connection );

            return nullptr;
        }

        template < typename TT, typename Connection >
        auto call_ll_phy_updated( TT& obj, Connection& connection )
            -> decltype(&TT::template ll_phy_updated< Connection >)
        {
            obj.ll_phy_updated(
                static_cast< bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t >( raw_details_[ 0 ] ),
                static_cast< bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t >( raw_details_[ 1 ] ),
                connection );

            return nullptr;
        }

        template < typename TT >
        void call_ll_connection_requested( ... )
        {
        }

        template < typename TT >
        void call_ll_connection_attempt_timeout( ... )
        {
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

        template < typename TT >
        void call_ll_rejected( ... )
        {
        }

        template < typename TT >
        void call_ll_unknown( ... )
        {
        }

        template < typename TT >
        void call_ll_remote_features( ... )
        {
        }

        template < typename TT >
        void call_ll_phy_updated( ... )
        {
        }

        /** @endcond */
    };

    namespace details {
        struct no_connection_callbacks {
            struct meta_type :
                details::connection_callbacks_meta_type,
                details::valid_link_layer_option_meta_type {};

            void connection_request( const connection_addresses& ) {}

            template < class Connection, class Radio >
            void connection_requested(
                const connection_details&,
                Connection&,
                Radio& ) {}

            template < class Connection, class Radio >
            void connection_attempt_timeout( Connection&, Radio& ) {}

            template < class Connection, class Radio >
            void connection_established(
                const connection_details&,
                Connection&,
                Radio& ) {}

            template < class Connection, class Radio >
            void connection_changed( const bluetoe::link_layer::connection_details&, Connection&, Radio& ) {}

            template < class Connection, class Radio >
            void connection_closed( std::uint8_t, Connection&, Radio& ) {}

            template < class LinkLayer >
            void handle_connection_events() {}

            template < class Connection, class Radio >
            void version_indication_received( const std::uint8_t*, Connection&, Radio& ) {}

            template < class Connection, class Radio >
            void procedure_rejected( std::uint8_t, Connection&, Radio& ) {}

            template < class Connection, class Radio >
            void procedure_unknown( std::uint8_t, Connection&, Radio& ) {}

            template < class Connection, class Radio >
            void remote_features_received( const std::uint8_t[8], Connection&, Radio& ) {}

        };
    }

}
}

#endif
