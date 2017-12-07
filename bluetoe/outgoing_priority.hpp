#ifndef BLUETOE_OUTGOING_PRIORITY_HPP
#define BLUETOE_OUTGOING_PRIORITY_HPP

#include <bluetoe/options.hpp>

namespace bluetoe {

    namespace details {
        struct outgoing_priority_meta_type {};

        template < typename Services, typename ServiceUUID >
        struct number_of_additional_priorities
        {
            template < typename Service >
            using has_uuid = std::is_same< typename Service::uuid, ServiceUUID >;

            // find the Service from the ServiceUUID in the List of Services
            using service = typename find_if< Services, has_uuid >::type;
            static constexpr int size = service::notification_priority::size;

            // if the number of named characteristics is equal to the number of characteristics in the service,
            // no characteristic will have default priority
            static constexpr int size_with_default = size == service::number_of_client_configs
                ? size
                : size + 1;

            // For a Service that doesn't contain a priority per characteristic, there is at least one
            // addition priority, because all characteristics in the service form have a new unique priority
            using type = std::integral_constant< int, size_with_default == 0 ? 1 : size_with_default >;
        };

        template < typename ... Us >
        struct check_server_parameter
        {
            static_assert( details::count_by_meta_type< details::service_uuid_meta_type, Us... >::count == sizeof...( Us ),
                "Only service UUIDs are acceptable parameters to higher_outgoing_priority<> as server parameter." );

            static constexpr bool check = true;
        };

        template < typename ... Us >
        struct check_service_parameter
        {
            static_assert( details::count_by_meta_type< details::characteristic_uuid_meta_type, Us... >::count == sizeof...( Us ),
                "Only characteristic UUIDs are acceptable parameters to higher_outgoing_priority<> as service parameter." );

            static constexpr bool check = true;
        };

        template < typename T, int Prio >
        struct add_prio;

        template <>
        struct add_prio< std::tuple<>, 0 >
        {
            using type = std::tuple< std::integral_constant< int, 1 > >;
        };

        template < int N, typename ...Ts >
        struct add_prio< std::tuple< std::integral_constant< int, N >, Ts... >, 0 >
        {
            using type = std::tuple< std::integral_constant< int, N + 1 >, Ts... >;
        };

        template < int Prio >
        struct add_prio< std::tuple<>, Prio >
        {
            using type = typename add_type<
                std::integral_constant< int, 0 >,
                typename add_prio< std::tuple<>, Prio -1 >::type >::type;
        };

        template < int N, typename ...Ts, int Prio >
        struct add_prio< std::tuple< std::integral_constant< int, N >, Ts... >, Prio >
        {
            using type = typename add_type<
                std::integral_constant< int, N >,
                typename add_prio< std::tuple< Ts... >, Prio -1 >::type >::type;
        };

    }

    /**
     * @brief Defines priorities of notified or indicated characteristics.
     *
     * In Bluetoe, when a characteristic notification or indication have to
     * be send, the server::notify or server::indicate function can be used
     * to queue this characteristic for beeing send out. Once the link layer
     * finds a free spot for a notification or indication, it will determin
     * one of the queued characteristics, reserve a buffer,
     * fill that buffer with the value of the characteristic and send it out.
     *
     * higher_outgoing_priority and lower_outgoing_priority can be used
     * define in which order queued notifications and indications are send.
     *
     * The higher_outgoing_priority option can be used as option to a
     * service, to define the priority of outgoing notifications and
     * indications based on the characteristics UUID. UUIDs are given in
     * decreasing order of priority. Characteristics of the service that
     * are not named in the list have a priority that is lower than the
     * last element in the list.
     *
     * The higher_outgoing_priority option can also be used as option to
     * a server, to raise the priority of all characteristics within a
     * service. Then, all characteristics within a service, that is not
     * named as a parameter to higher_outgoing_priority have a priority
     * that is less than all characteristics within the last element of
     * the list.
     *
     * Example:
     * Given that we have 3 services A, B, and C which all contain 3
     * characteristics a, b, and c. Given this pseudo GATT server:
     * @code
     using server = bluetoe::server<
        bluetoe::service<
            A,
            bluetoe::characteristic< a, bluetoe::notify >,
            bluetoe::characteristic< b, bluetoe::notify >,
            bluetoe::characteristic< c, bluetoe::notify >,
            bluetoe::higher_outgoing_priority< b >
        >,
        bluetoe::service<
            B,
            bluetoe::characteristic< a, bluetoe::notify >,
            bluetoe::characteristic< b, bluetoe::notify >,
            bluetoe::characteristic< c, bluetoe::notify >,
            bluetoe::lower_outgoing_priority< a >
        >,
        bluetoe::service<
            C,
            bluetoe::characteristic< a, bluetoe::notify >,
            bluetoe::characteristic< b, bluetoe::notify >,
            bluetoe::characteristic< c, bluetoe::notify >,
            bluetoe::higher_outgoing_priority< a, b >
        >,
        bluetoe::higher_outgoing_priority< A >
     >;
     * @endcode
     *
     * Now, the priority of each characteristic is as follows. Characteristics comming first have
     * higher priorities. Characteristics on the same line have equal priority.
     * @code
     Service:  | A       | B       | C
     ---------------------------------------
     highest   | b       |         |
               | a,c     |         |
               |         |         | a
               |         |         | b
               |         | b, c    | c
     lowest    |         | a       |
     * @endcode
     *
     * Note, that the characteristics b and c of service B and the characteristic c of service
     * C have the very same priority, because B and C have the same priority and the characteristics
     * have default (unchanged) priorities.
     *
     * @sa lower_outgoing_priority
     */
    template < typename ... UUIDs >
    struct higher_outgoing_priority {
        /** @cond HIDDEN_SYMBOLS */
        using meta_type = details::outgoing_priority_meta_type;

        template < typename Services, typename Service >
        struct service_base_priority
        {
            template < typename Sum, typename ServiceUUID >
            struct optional_sum_prio
            {
                using found = std::integral_constant< bool, Sum::first_type::value || std::is_same< ServiceUUID, typename Service::uuid >::value >;
                using prio  = typename Sum::second_type;

                // Only sum up the priorities that are before ServiceUUID in UUIDs...
                using sum   = typename details::select_type< found::value,
                    prio,
                    typename details::number_of_additional_priorities< Services, ServiceUUID >::type >::type;

                using type = details::pair< found, sum >;
            };

            // while Service::uuid is not in UUIDs... sum up the priorities of the services
            using type = typename details::fold_left<
                                        std::tuple< UUIDs... >,
                                        optional_sum_prio,
                                        details::pair< std::false_type, std::integral_constant< int, 0 > > >::type;

            static constexpr int value = type::second_type::value;
        };

        template < typename Sum, typename Service >
        struct expand_shared_priorities
        {
            static constexpr int sum   = Sum::value;

            // is this one of the services that share the priorities?
            static constexpr bool shared = details::index_of< typename Service::uuid, UUIDs... >::value == sizeof...( UUIDs );

            static constexpr int prios = Service::notification_priority::size;

            static constexpr int shared_value = prios > sum ? prios : sum;
            static constexpr int value = shared ? shared_value : sum;

            using type = std::integral_constant< int, value >;
        };

        // this function is called on the server instantiation
        template < typename Services, typename Service, typename Characteristic >
        struct characteristic_priority
        {
            static constexpr auto checked = details::check_server_parameter< UUIDs... >::check;

            static constexpr int service_prio = service_base_priority< Services, Service >::value;

            // there is a big difference whether a Service is within UUIDs or not. In case, a Service is not within UUIDs
            // (Service B and C in the example above), characteristics from different services can share the same priority.
            static constexpr bool priorized_service = details::index_of< typename Service::uuid, UUIDs... >::value != sizeof...( UUIDs );

            // the maximum of introduced priorities of all services that are not within UUIDs (and thus share a range of priorities).
            static constexpr int shared_priorities = details::fold< Services, expand_shared_priorities, std::integral_constant< int, 0 > >::type::value;

            static constexpr int char_prio    = Service::notification_priority::template characteristic_position< Characteristic, priorized_service, shared_priorities >::value;

            static constexpr int value = service_prio + char_prio;
        };

        template < typename Services, typename Service >
        struct numbers_from_char
        {
            template < typename Numbers, typename Characteristic >
            struct impl
            {
                static constexpr int prio = characteristic_priority< Services, Service, Characteristic >::value;
                using type = typename details::add_prio< Numbers, prio >::type;
            };
        };

        // Returns a list of numbers of priorities, starting with the number of characteristics with prio 0
        template < typename Services >
        struct numbers
        {
            template < typename List, typename Characteristic >
            using characteristics_with_cccd = details::select_type<
                    Characteristic::number_of_client_configs,
                    typename details::add_type< List, Characteristic >::type,
                    List >;

            template < typename Numbers, typename Service >
            using numbers_from_services = details::fold<
                typename details::fold<
                    typename Service::characteristics,
                    characteristics_with_cccd >::type,
                numbers_from_char< Services, Service >::template impl,
                Numbers >;

            using type = typename details::fold< Services, numbers_from_services, std::tuple<> >::type;
        };

        static constexpr std::size_t size = sizeof...( UUIDs );

        // this function is called on the service instantiation
        template < typename Characteristic, bool WithinPriorizedService, int SharedPriorities >
        struct characteristic_position
        {
            static constexpr auto checked = details::check_service_parameter< UUIDs... >::check;

            static constexpr int pos = details::index_of< typename Characteristic::configured_uuid, UUIDs... >::value;
            static constexpr int value = WithinPriorizedService
                ? pos
                : pos + SharedPriorities - size;
        };

        /** @endcond */
    };

    /**
     * @brief Defines priorities of notified or indicated characteristics.
     * @sa higher_outgoing_priority
     * @attention currently not implemented
     */
    template < class ... UUIDs >
    struct lower_outgoing_priority {
        /** @cond HIDDEN_SYMBOLS */
        using meta_type = details::outgoing_priority_meta_type;
        /** @endcond */
    };

    /**
     * @example priorities_example.cpp
     * This examples shows, how to define priorities for charactertic notifications and indications.
     */

}


#endif
