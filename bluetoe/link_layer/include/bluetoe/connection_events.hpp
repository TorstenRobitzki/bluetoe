#ifndef BLUETOE_LINK_LAYER_CONNECTION_EVENT_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_EVENT_HPP

namespace bluetoe {
namespace link_layer {

    /**
     * @brief set of events, that could have happend during a connection event
     *
     * This is used by the radio to give some details about, what happend in the last
     * connection event.
     */
    struct connection_event_events
    {
        /**
         * @brief The last, not empty PDU, that was send out during the connection event
         *        was _not_ acknowlaged by the central yet.
         */
        bool unacknowledged_data;

        /**
         * @brief At the last connection event, there was at least one not empty PDU received from
         *        the central.
         */
        bool last_received_not_empty;

        /**
         * @brief At the last connection event, there was at least one PDU transmitted, that was not empty.
         */
        bool last_transmitted_not_empty;

        /**
         * @brief The last PDU received at the connection event, had the MD flag beeing set.
         */
        bool last_received_had_more_data;

        /**
         * @brief c'tor to reset all flags
         */
        connection_event_events()
            : unacknowledged_data( false )
            , last_received_not_empty( false )
            , last_transmitted_not_empty( false )
            , last_received_had_more_data( false )
        {
        }
    };

}
}

#endif
