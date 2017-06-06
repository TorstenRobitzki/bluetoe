#ifndef BLUETOE_OUTGOING_PRIORITY_HPP
#define BLUETOE_OUTGOING_PRIORITY_HPP

namespace bluetoe {

    /**
     * @brief Defines priority of notified or indicated characteristics.
     *
     * The higher_outgoing_priority option can be used as option to a
     * service, to define the priority of outgoing notifications and
     * indications based on the characteristics UUID. UUIDs are given in
     * decreasing order of priority. Characteristics of the service that
     * are not named in the list have a priority that is lower than the
     * last element in the list.
     *
     * The higher_outgoing_priority option can also be used as option to
     * a server, to define
     *
     */
    template < class ... UUIDs >
    struct higher_outgoing_priority {

    };

    template < class ... UUIDs >
    struct lower_outgoing_priority {

    };

    /**
     * @example priorities_example.cpp
     * This examples shows, how to define priorities for charactertic notifications and indications.
     */

}


#endif
