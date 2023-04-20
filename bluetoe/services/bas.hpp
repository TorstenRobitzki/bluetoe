#ifndef BLUTOE_SERVICES_BAS_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/mixin.hpp>
#include <type_traits>

namespace bluetoe
{
    namespace bas
    {
        /**
         * @brief UUID of a Battery Service (BAS)
         */
        using service_uuid = service_uuid16< 0x180F >;

        /**
         * @brief UUID of a level characteristic
         */
        using level_uuid = characteristic_uuid16< 0x2A19 >;

        namespace details {
            /** @cond HIDDEN_SYMBOLS */
            struct handler_tag {};
            struct no_battery_level_notifications_tag {};
            struct valid_bas_service_option_tag {};

            template < typename Handler >
            struct handler_impl
            {
                struct read_handler : bluetoe::details::value_handler_base
                {
                    template < class Server >
                    static std::uint8_t call_read_handler( std::size_t offset, std::size_t, std::uint8_t* out_buffer, std::size_t& out_size, void* server )
                    {
                        if ( offset != 0 )
                            return bluetoe::error_codes::attribute_not_long;

                        Handler& handler = static_cast< Handler& >( *static_cast< Server* >( server ) );

                        const int value = handler.read_battery_level();
                        assert(value >= 0);
                        assert(value <= 100);

                        *out_buffer = static_cast< std::uint8_t >( value );
                        out_size = 1;

                        return bluetoe::error_codes::success;
                    }

                    struct meta_type :
                        bluetoe::details::value_handler_base::meta_type,
                        bluetoe::details::characteristic_value_read_handler_meta_type {};
                };

                template < typename Server >
                void notifiy_battery_level( Server& srv )
                {
                    srv.template notify< level_uuid >();
                }
            };

            template < typename, typename >
            struct service_impl;

            template < typename Handler, typename ... Options >
            struct service_impl< Handler, std::tuple< Options... > >
            {
                using type =
                    bluetoe::service<
                        service_uuid,
                        bluetoe::characteristic<
                            level_uuid,
                            typename handler_impl< Handler >::read_handler,
                            bluetoe::notify,
                            bluetoe::notify_on_subscription
                        >,
                        bluetoe::mixin< handler_impl< Handler > >,
                        Options...
                    >;
            };


            template < typename ... Options >
            struct calculate_service {
                using handler = typename bluetoe::details::find_by_meta_type< handler_tag, Options... >::type;

                static_assert( !std::is_same< handler, bluetoe::details::no_such_type >::value, "bas::handler<> is required" );

                using other_args = typename bluetoe::details::find_all_by_not_meta_type<
                    details::valid_bas_service_option_tag, Options... >::type;

                using type = typename service_impl< typename handler::type, other_args >::type;
            };
            /** @endcond */
        }

        /**
         * @brief indicates no support for battery level notifications
         *
         * If this option is given to battery_level, then the implementation of the server
         * will not implement notifications for this service.
         *
         * @todo Not implemented yet
         */
        struct no_battery_level_notifications {
            /** @cond HIDDEN_SYMBOLS */
            struct meta_type : details::no_battery_level_notifications_tag
                             , details::valid_bas_service_option_tag {};
            /** @endcond */
        };

        /**
         * @brief pass a handler to the service
         *
         * The handler have to provide a single function with the following name and type:
         *
         * @code
         * int read_battery_level();
         * @endcode
         *
         * Use bluetoe::mixin to inject an instance of that handler into the GATT server.
         */
        template < typename Handler >
        struct handler {
            /** @cond HIDDEN_SYMBOLS */
            using type = Handler;

            struct meta_type : details::handler_tag
                             , details::valid_bas_service_option_tag {};
            /** @endcond */
        };
    }

    /**
     * @brief implementation of the Battery Service
     *
     * To adapt the measured battery level to the GATT service,
     * a handler have to be provided, that can be called, to
     * measure the battery level.
     *
     * A bluetoe::server<> instance that contains a battery service, implements
     * a function `void notifiy_battery_level(GATTServer&)`, that will request a notification
     * of the battery level characteristic. If the no_battery_level_notifications
     * option is given, this function will not be implemeted.
     *
     * @sa bluetoe::bas::no_battery_level_notifications
     * @sa bluetoe::bas::handler
     */
    template < typename ... Options >
    using battery_level = typename bas::details::calculate_service< Options... >::type;
}

#endif
