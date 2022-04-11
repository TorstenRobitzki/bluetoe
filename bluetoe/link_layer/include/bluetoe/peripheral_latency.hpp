#ifndef BLUETOE_LINK_LAYER_PERIPHERAL_LATENCY_HPP
#define BLUETOE_LINK_LAYER_PERIPHERAL_LATENCY_HPP

#include <bluetoe/ll_meta_types.hpp>
#include <bluetoe/connection_events.hpp>

/**
 * @file bluetoe/peripheral_latency.hpp
 *
 * This file provides several options, that allow users to configure Bluetoe's
 * support for peripheral latency.
 * Peripheral latency allows a link layer peripheral to ignore some connection events
 * (do not respond to the central) to conserve power.
 *
 * Each option is a compromise between latency and power consumption.
 *
 * @sa bluetoe::link_layer::peripheral_latency_ignored
 * @sa ...
 */
namespace bluetoe {
namespace link_layer {

    namespace details {
        struct peripheral_latency_meta_type {};
    }

    /**
     * @brief detailed options for the peripheral latency behavior
     *
     * Every option defines a set of events / circumstances under which
     * the link layer should listen at the very next connection event.
     * Regardless of the selected options, the link layer always listens
     * for incoming PDU at each peripheral latency anchor points, which
     * were negotiated as part of the current connection parameters.
     *
     * @note in combination, there are some options that make no sense.
     *       For example, using listen_always with some of the other
     *       listen_* options is pointless.
     *       TODO: Specify what happens if listen always is combined with
     *       listen_if_pending_data ?
     */
    enum class peripheral_latency
    {
        /**
         * @brief listen at the very next connection event if bluetoe
         * has an available PDU for sending. This reduces the latency
         * of any available payload.
         */
        listen_if_pending_transmit_data,

        /**
         * @brief listen if the link layer contains unacknowledged data.
         *
         * If a PDU was sent, but no acknowledgement has been received,
         * the link layer will listen at the next connection event.
         *
         * This might be interesting, if the sending queue is very small because
         * Bluetoe has to keep a copy of a transmitted PDU, as long as that PDU
         * is not acknowledged.
         */
        listen_if_unacknowledged_data,

        /**
         * @brief listen to the next connection event, if the last received PDU was
         *        not empty.
         */
        listen_if_last_received_not_empty,

        /**
         * @brief listen to the next connection event, if the last transmitted PDU was
         *        not empty.
         */
        listen_if_last_transmitted_not_empty,

        /**
         * @brief listen to the next connection event, if the last received PDU had
         *        the more data (MD) flag set.
         *
         * With the MD flag, the central (and the peripheral) can indicate, that there
         * is more data to be sent. Usually this data is then transmitted during the same
         * connection event. If there is no support for handling more data in the same
         * connection event either by the peripheral or by the central, then this option
         * may help reducing the latency of the pending data.
         */
        listen_if_last_received_had_more_data,

        /**
         * @brief listen at all connection events, despite the negotiated
         *        peripheral latency value of the current connection.
         */
        listen_always,

    };

    /**
     * @brief defines a peripheral configuration to be used by the link layer
     *
     * The configuration is a set of options from peripheral_latency. Example:
     *
     * @code{.cpp}
     * bluetoe::device<
     *    gatt_server_definition,
     *    bluetoe::link_layer::peripheral_latency_configuration<
     *        bluetoe::link_layer::peripheral_latency::listen_if_unacknowledged_data,
     *        bluetoe::link_layer::peripheral_latency::listen_if_last_received_had_error
     *    >
     * > gatt_server;
     * @endcode
     *
     * @sa peripheral_latency
     * @sa peripheral_latency_configuration_set
     */
    template < peripheral_latency ... Options >
    struct peripheral_latency_configuration
    {
        /** @cond HIDDEN_SYMBOLS */

        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief allows the peripheral latency configuration to be changed at runtime
     *        between a given set of configurations.
     *
     * Every given Configuration has to be a vaild peripheral_latency_configuration<>.
     *
     * For example:
     *
     * @code{.cpp}
     * bluetoe::device<
     *    gatt_server_definition,
     *    bluetoe::link_layer::peripheral_latency_configuration_set<
     *        bluetoe::link_layer::peripheral_latency::peripheral_latency_ignored,
     *        bluetoe::link_layer::peripheral_latency::peripheral_latency_strict_plus >
     *    >
     * > gatt_server;
     *
     * void foo()
     * {
     *     gatt_server.change_peripheral_latency<
     *         bluetoe::link_layer::peripheral_latency::peripheral_latency_ignored >();
     * }
     * @endcode
     *
     * @sa peripheral_latency_configuration
     */
    template < typename ... Configuration >
    class peripheral_latency_configuration_set
    {
    public:
        template < typename NewConfig >
        void change_peripheral_latency();

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief Instructs the link layer to ignore peripheral latency
     *
     * The link layer will listen at every connection event and will respond at every
     * connection event.
     *
     * This option will provide lowest latency without any chance to conserve power.
     *
     * @sa peripheral_latency_configuration
     */
    struct peripheral_latency_ignored : peripheral_latency_configuration<
        peripheral_latency::listen_always
    >
    {
    };

    /**
     * @brief Configure the link layer to only listen every configured connection event.
     *
     * The link layer will only listen to the central every "peripheral latency" + 1 connection
     * events. If for example, the peripheral latency of the current connection is 4, the link
     * layer will just listen to every 5th connection event.
     *
     * If there is pending, outgoing data, the link layer will start to listen on the next possible
     * connection event to be able to reply with the pending data. Also, if a received PDU has the
     * more data (MD) flag beeing set, the link layer will start listening at the next connection event.
     *
     * This option provided lowest power consumption while providing better transmitting latency compared
     * to having a connection interval equal to peripheral latency + 1 * interval.
     *
     * @sa peripheral_latency_always_listening
     */
    struct peripheral_latency_strict : peripheral_latency_configuration<
        peripheral_latency::listen_if_last_received_had_more_data
    >
    {
    };

    /**
     * @brief Configure the link layer to just listen every configured connection event
     *        and on the next connection events if the previous connection event contained data
     *        from the central.
     *
     * This option extends the peripheral_latency_strict option by listening
     * on subsequent events, if there was data received at the previous event.
     *
     * This option provided lowest power consumption while providing better transmitting latency compared
     * to having a connection interval equal to peripheral latency + 1 * interval. Compared to
     * peripheral_latency_strict, this might increase the receiving bandwidth.
     *
     * @sa peripheral_latency_always_listening
     * @sa peripheral_latency_strict
     */
    struct peripheral_latency_strict_plus : peripheral_latency_configuration<
        peripheral_latency::listen_if_last_received_not_empty,
        peripheral_latency::listen_if_last_received_had_more_data
    >
    {
    };

    namespace details {

        /**
         * @brief book keeping for the current / next connection event
         */
        template < typename PeripheralLatencyOptionSet >
        class connection_state : private PeripheralLatencyOptionSet
        {
        public:
            /**
             * @brief Plan next connection event after timeout
             */
            void plan_next_connection_event_after_timeout(
                std::uint16_t           connection_peripheral_latency,
                delta_time              connection_interval );

            /**
             * @brief Plan next connection event
             */
            void plan_next_connection_event(
                std::uint16_t           connection_peripheral_latency,
                connection_event_events last_event_events,
                delta_time              connection_interval,
                delta_time              now );

            /**
             * @brief connection is just established
             */
            void reset_connection_state();

            /**
             * @brief index into the channel map for the next, planned connection event
             */
            unsigned      current_channel_index() const;

            /**
             * @brief index into the channel map for the next, planned connection event
             */
            std::uint16_t connection_event_counter() const;
            delta_time    time_till_next_event() const;

        private:
            unsigned        channel_index_;
            std::uint16_t   event_counter_;
            delta_time      time_till_next_event_;
        };

        // implementation
        template < typename PeripheralLatencyOptionSet >
        void connection_state< PeripheralLatencyOptionSet >::plan_next_connection_event_after_timeout(
            std::uint16_t           ,
            delta_time              connection_interval )
        {
            time_till_next_event_ += connection_interval;
            channel_index_ = ( channel_index_ + 1 ) % channel_map::max_number_of_data_channels;
            ++event_counter_;
        }

        template < typename PeripheralLatencyOptionSet >
        void connection_state< PeripheralLatencyOptionSet >::plan_next_connection_event(
            std::uint16_t           connection_peripheral_latency,
            connection_event_events last_event_events,
            delta_time              connection_interval,
            delta_time              now )
        {
            static_cast< void >( connection_peripheral_latency );
            static_cast< void >( last_event_events );
            static_cast< void >( now );
            time_till_next_event_ = connection_interval;
            channel_index_ = ( channel_index_ + 1 ) % channel_map::max_number_of_data_channels;
            ++event_counter_;
        }

        template < typename PeripheralLatencyOptionSet >
        void connection_state< PeripheralLatencyOptionSet >::reset_connection_state()
        {
            channel_index_        = 0;
            event_counter_        = 0;
            time_till_next_event_ = delta_time();
        }

        template < typename PeripheralLatencyOptionSet >
        unsigned connection_state< PeripheralLatencyOptionSet >::current_channel_index() const
        {
            return channel_index_;
        }

        template < typename PeripheralLatencyOptionSet >
        std::uint16_t connection_state< PeripheralLatencyOptionSet >::connection_event_counter() const
        {
            return event_counter_;
        }

        template < typename PeripheralLatencyOptionSet >
        delta_time connection_state< PeripheralLatencyOptionSet >::time_till_next_event() const
        {
            return time_till_next_event_;
        }

    }
}
}

#endif
