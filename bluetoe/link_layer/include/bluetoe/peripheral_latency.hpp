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
     * @brief Instructs the link layer to ignore peripheral latency
     *
     * The link layer will listen at every connection event and will respond at every
     * connection event.
     *
     * This option will provide lowest latency without any chance to conserve power.
     */
    struct peripheral_latency_ignored
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
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
     */
    struct peripheral_latency_always_listening
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief Configured the link layer to always listen at connection events and
     *        once, data was received at the last event
     *
     * This option extends the peripheral_latency_always_listening option by listening
     * on subsequent events, if there was data received at the previous event.
     *
     * This option provides lowest latency with some power conserved.
     */
    struct peripheral_latency_always_listening_plus
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
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
     */
    struct peripheral_latency_strict
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
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
     */
    struct peripheral_latency_strict_plus
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };
}
}

#endif
