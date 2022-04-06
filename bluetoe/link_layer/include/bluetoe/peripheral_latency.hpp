#ifndef BLUETOE_LINK_LAYER_PERIPHERAL_LATENCY_HPP
#define BLUETOE_LINK_LAYER_PERIPHERAL_LATENCY_HPP

#include <bluetoe/ll_meta_types.hpp>

/**
 * @file bluetoe/peripheral_latency.hpp
 *
 * This file provides several options, how Bluetoe will implement peripheral latency. Peripheral latency
 * allows a link layer peripheral to miss some connection events (do not respond to the central) to
 * conserve power.
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
     * the link layer should listen to the very next connection event.
     * (beside the minumum events, the link layer has to listen to due
     * to the periperhal latency value of the connection).
     *
     * @note in combination, there are some options that make no sense.
     *       For example, using listen_always with some of the other
     *       listen_* options is pointless. Also combining send_always
     *       with send_pending_data doesn not make sense.
     */
    enum class peripheral_latency
    {
        /**
         * @brief listen at all connection events.
         *
         * This does not imply, that the link layer will also respond at every
         * connection event.
         */
        listen_always,

        /**
         * @brief listen, if the link layer contains unacknowlaged data.
         *
         * If a PDU was send out, but no acknowlagement was jet received,
         * the link layer will listen at the next connection event, if
         * this options is set.
         *
         * This might be interesting, if the sending queue is very small because
         * Bluetoe has to keep a copy of a transmitted PDU, as long as that PDU
         * is not acknowlaged.
         */
        listen_if_unacknowlaged_data,

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
         * is more data to be send. Usually this data is then transmitted during a
         * connection even. If there is no support for the MD flag either by the peripheral
         * or by the central, then, this option might be usefull.
         */
        listen_if_last_received_had_more_data,

        /**
         * @brief listen to the next connection event, if for the last connection event,
         *        a crc error happend, or a timeout occured.
         *
         * This behavior is required by the Bluetooth specification and should not left out
         * without good reason.
         */
        listen_if_last_received_had_error,

        /**
         * @brief always respond to a first PDU from the central at a connection event.
         *
         * If the link layer listened to the connection event and received a valid PDU
         * from the central, it will reply.
         */
        send_always,

        /**
         * @brief if there is pending data to be send out, the stack will send that
         *        data at the next scheduled connection event.
         *
         * If the stack is configured to use peripheral latency and decided to leave out
         * a few connection events, and then data to be send out becomed available, the
         * next scheduled event will not be changed. If the desired behaviour is, to send
         * the data out with the very next connection event, this option has to be used
         * in conjunction with `listen_always`.
         */
        send_pending_data,

        /**
         * @brief if the last received PDU has the MD flag set, the link layer
         *        will acknowlage that PDU
         *
         * The link layer will acknowlage the received PDU, even if it is empty and
         * the peripheral latency would not force a response.
         *
         * In case, peripheral latency is used, it might happen, that the central send
         * out an empty PDU. If this PDU is not acknowlaged by the peripheral, the
         * central has to resend that empty PDU until it is acknowlaged, even if
         * there is data to be send from the central to the peripheral. In such a
         * case, it is likely, that the central will set its MD flag to indicate
         * that indeed there is data pending to be send to the peripheral.
         */
        send_if_last_received_had_more_data,

        /**
         * @brief if the last received PDU was not empty, the link layer
         *        will acknowlage that PDU.
         */
        send_if_last_received_not_empty,
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
     *        bluetoe::link_layer::peripheral_latency::listen_if_unacknowlaged_data,
     *        bluetoe::link_layer::peripheral_latency::listen_if_last_received_had_error,
     *        bluetoe::link_layer::peripheral_latency::send_pending_data
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
     * @brief allows to change the peripheral latency configuration to be changed at runtime
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
        peripheral_latency::listen_always,
        peripheral_latency::send_always
    >
    {
    };


    /**
     * @brief Configured the link layer to always listen at connection events
     *
     * The link layer will listen at every connection event and will respond only, if
     * the More Data flag of incomming PDUs is set, if an incomming PDU is not empty,
     * of if the peripheral has pending data to be send. (and of cause, if the pheriphal
     * has not responded for the configured amount of events).
     *
     * This option provides lowest latency with some power conserved.
     *
     * @sa peripheral_latency_configuration
     */
    struct peripheral_latency_always_listening : peripheral_latency_configuration<
        peripheral_latency::listen_always,
        peripheral_latency::listen_if_last_received_had_error,
        peripheral_latency::send_if_last_received_had_more_data,
        peripheral_latency::send_if_last_received_not_empty,
        peripheral_latency::send_pending_data
    >
    {
    };

    /**
     * @brief Configured the link layer to always listen at connection events and
     *        once, data was received at the last event
     *
     * This option extends the peripheral_latency_always_listening option by listening
     * on subsequent events, if there was data received at the previous event.
     *
     * This option provides lowest latency with some power conserved.
     *
     * @sa peripheral_latency_always_listening
     * @sa peripheral_latency_configuration
     */
    struct peripheral_latency_always_listening_plus : peripheral_latency_configuration<
        peripheral_latency::listen_always,
        peripheral_latency::listen_if_last_received_had_error,
        peripheral_latency::listen_if_last_received_not_empty,
        peripheral_latency::send_if_last_received_had_more_data,
        peripheral_latency::send_if_last_received_not_empty,
        peripheral_latency::send_pending_data
    >
    {
    };

    /**
     * @brief Configure the link layer to just listen every configured connection event.
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
        peripheral_latency::listen_if_last_received_had_error,
        peripheral_latency::listen_if_last_received_had_more_data,
        peripheral_latency::send_pending_data
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
        peripheral_latency::listen_if_last_received_had_error,
        peripheral_latency::listen_if_last_received_not_empty,
        peripheral_latency::listen_if_last_received_had_more_data,
        peripheral_latency::send_pending_data
    >
    {
    };
}
}

#endif
