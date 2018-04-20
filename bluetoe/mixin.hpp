#ifndef BLUETOE_MIXIN_HPP
#define BLUETOE_MIXIN_HPP

#include <bluetoe/options.hpp>

namespace bluetoe {
    template < typename ... T >
    struct mixin;

    template < typename ... Options >
    class server;

    namespace details {
        struct mixin_meta_type {};

        template < typename MixinList >
        struct sum_mixins;

        template <>
        struct sum_mixins< std::tuple<> >
        {
            typedef std::tuple<> type;
        };

        template < typename ... Ts, typename ... Ms >
        struct sum_mixins< std::tuple< mixin< Ts... >, Ms... > >{
            typedef typename add_type<
                std::tuple< Ts... >,
                typename sum_mixins< std::tuple< Ms... > >::type >::type type;
        };

        template < typename List, typename Element >
        struct extract_service_mixins;

        template < typename ... Ms, typename ... Options >
        struct extract_service_mixins< std::tuple< Ms... >, bluetoe::service< Options... > >
        {
            typedef typename find_all_by_meta_type< mixin_meta_type, Options... >::type mixins;
            typedef typename add_type< mixins, std::tuple< Ms... > >::type              type;
        };

        template < typename ... Ms, typename T >
        struct extract_service_mixins< std::tuple< Ms... >, T >
        {
            typedef std::tuple< Ms... > type;
        };

        template < typename ... Options >
        struct collect_mixins
        {
            typedef typename find_all_by_meta_type< mixin_meta_type, Options... >::type         server_mixins;
            typedef typename fold< std::tuple< Options ... >, extract_service_mixins >::type    service_mixins;
            typedef typename add_type< server_mixins, service_mixins >::type                mixins;
            typedef typename sum_mixins< mixins >::type                                     type;
        };

    }

    /**
     * @brief class to be mixed into the server instance
     *
     * This option can be passed to a bluetoe::server or to a bluetoe::service. The bluetoe::server will derive from the given
     * types in the given order. If the option is used multiple times, the server will first derive from the type given to the
     * server and then from the type given to the services in the same order as the services are passed to the server.
     *
     * As there is no way defined to pass constructor arguments, all types have to have default constructors.
     *
     * @sa server
     * @sa service
     * @sa mixin_read_handler
     * @sa mixin_write_handler
     * @sa mixin_read_blob_handler
     * @sa mixin_write_blob_handler
     * @sa mixin_write_indication_control_point_handler
     * @sa mixin_write_notification_control_point_handler
     */
    template < typename ... T >
    struct mixin {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::mixin_meta_type meta_type;
        /** @endcond */
    };


}
#endif
