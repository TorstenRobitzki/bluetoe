#ifndef BLUETOE_OUTGOING_PRIORITY_HPP
#define BLUETOE_OUTGOING_PRIORITY_HPP

namespace bluetoe {

    namespace details {
        struct outgoing_priority_service_meta_type {};
        struct outgoing_priority_characteristic_meta_type {};
        struct outgoing_priority_meta_type {};
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
    template < class ... UUIDs >
    struct higher_outgoing_priority {
        /** @cond HIDDEN_SYMBOLS */
        using meta_type = details::outgoing_priority_meta_type;

        template < class MetaType, class Entities >
        struct impl;

        template < class Char, class ... Chars >
        struct impl< details::outgoing_priority_characteristic_meta_type, std::tuple< Char, Chars... > >
        {
            using ordered_by_priority = std::tuple< Char, Chars... >;
            using priorites           = std::tuple< std::integral_constant< std::size_t, sizeof ...(Chars) + 1 > >;
        };
        /** @endcond */
    };

    /**
     * @brief Defines priorities of notified or indicated characteristics.
     * @sa higher_outgoing_priority
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
