#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO2_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO2_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/connection_events.hpp>
#include <bluetoe/security_connection_data.hpp>
#include <bluetoe/phy_encodings.hpp>
#include <bluetoe/abs_time.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

/**
 * @file scheduled_radio2.hpp
 *
 * Requirements of a scheduled radio, the abstraction of a radio hardware combined with a
 * timer that a peripheral link layer is built on, stated as C++20 concepts. A scheduled
 * radio is a template over the type it delivers its callbacks to, and the concept is over
 * that template together with that type: scheduled_radio< Radio, CallBacks >. A link
 * layer is a template over a radio template, and checks the pair where both are complete.
 *
 * A concept checks syntax: that the functions exist with these signatures and these
 * results. Everything else this interface promises, what appears on air, at what time,
 * within what tolerance, and from which context, is stated in the comments next to each
 * requirement, and is what the test rig in tests/scheduled_radio/ checks. See
 * documentation/scheduled_radio_test_rig.md for the reasoning behind the decisions.
 *
 * @section time Time
 *
 * The interface has no function that returns the current time. Every abs_time an
 * implementation hands out is the time of something that happened on the radio, given
 * to the callback that reports it, and every time a caller passes in is derived from one
 * of those. A radio that is idle may have only its low frequency clock running, so a
 * time it produced on demand would either be coarse or force the high frequency clock
 * on; inside such a callback the radio has just produced a timestamp and is known to
 * have a clock.
 *
 * The consequence for a caller is that a sequence of radio actions starts with an action
 * that names no time, start_advertising(), and continues with actions whose times are
 * relative to the callbacks the earlier ones produced. Decision 15.
 *
 * @section contexts Contexts
 *
 * Three contexts exist, named by who lives in them. The radio context is the
 * implementation's own interrupt context. The link layer context is where the callbacks
 * are delivered and where the scheduling functions are called from. The application
 * context is where run() executes and where the GATT layer delivers its callbacks. Every
 * function and callback states which of them it belongs to: application, link layer,
 * radio, or any.
 *
 * A radio without hardware_supports_link_layer_context delivers the callbacks from inside
 * run(), so the link layer context and the application context are the same. A radio
 * with it provides the link layer context itself, as an interrupt below the radio's
 * priority, and the callbacks arrive from there while run() only sleeps. The contract is
 * the same in both cases; what changes is whether two of the three names denote one
 * context.
 *
 * The link layer's own state shared between its two contexts, data on its way from the
 * application to the PDU buffer and received data on its way up, is protected with
 * lock_guard. The radio's state is never the caller's concern: every scheduling function
 * is called from one context only, except start_advertising(), which the radio makes safe
 * itself. Decision 17.
 */

namespace bluetoe {
namespace link_layer {

    /**
     * @brief the callbacks of advertising and of the timer
     *
     * What every type a scheduled radio delivers to provides. Context: link layer. The
     * white list check that schedule_advertising_event() refers to is a radio context
     * callback of advertising; it is not required here yet.
     */
    template < typename T >
    concept scheduled_radio_callbacks = requires (
        T                               callbacks,
        abs_time                        when,
        const read_buffer&              received )
    {
        /*
         * Called exactly once, when the radio is ready to operate; the delay before it
         * lets the implementation wait for PLLs to settle or entropy to be collected
         * without polling. No scheduling function is called before it.
         *
         * Carries no time. Nothing has happened on the radio yet, and the link layer's
         * first action is start_advertising(), which needs none, so an implementation
         * is not required to run a clock before the radio is used.
         */
        callbacks.radio_ready();

        /*
         * An advertising event received a response. `when` is the time the first bit of
         * the response was on air; `received` is the buffer passed to the scheduling
         * function, filled.
         */
        callbacks.adv_received( when, received );

        /*
         * An advertising event received no response within its window.
         */
        callbacks.adv_timeout( when );

        /*
         * The timer scheduled with schedule_timer() expired. `when` is the time it was
         * scheduled for, which may differ from the time it is delivered at.
         */
        callbacks.user_timer( when );
    };

    /**
     * @brief the callbacks of connection events
     *
     * Required, in addition to scheduled_radio_callbacks, of a type that schedules
     * connection events; a link layer checks it where it does. Context: link layer, except
     * link_layer_pdu_buffer(), which is called from the radio context and has to return
     * immediately.
     */
    template < typename T >
    concept scheduled_radio_connection_callbacks = requires (
        T                               callbacks,
        abs_time                        when,
        connection_event_events         events )
    {
        /*
         * A connection event received nothing between its start and end times.
         */
        callbacks.connection_timeout( when );

        /*
         * A connection event took place. `when` is the time the event started, that is
         * the anchor the next event is computed from.
         */
        callbacks.connection_end_event( when, events );

        /*
         * The PDU buffer of the current connection, from which the radio takes outgoing
         * PDUs and into which it stores received ones during a connection event.
         *
         * Context: radio. Called between two PDUs of a connection event, with the inter
         * frame space to spare, so it returns at once and touches nothing but the buffer.
         * The buffer is read and written from the radio context while the link layer
         * fills and drains it from the link layer context, which it has to be built for.
         */
        requires std::is_lvalue_reference_v< decltype( callbacks.link_layer_pdu_buffer() ) >;
    };

    /**
     * @brief the functions the security manager needs for LE Secure Connections pairing
     *
     * Required of a scheduled_radio whose hardware_supports_lesc_pairing is true. Every
     * function is the one of the Core Specification of the same name.
     *
     * Context: any, and reentrant. These are pure computations on their arguments, and
     * long ones: a point multiplication takes hundreds of milliseconds on a small core.
     * Where the security manager runs them is its decision.
     *
     * The pointer arguments denote values whose size the specification fixes: u and v are
     * the 32 byte x coordinates of the two public keys, a private key is 32 bytes and a
     * public key 64. They stay pointers because a coordinate is a part of a larger key,
     * and an array parameter would force a copy on every call.
     */
    template < typename T >
    concept lesc_pairing_toolbox = requires (
        T                                           toolbox,
        const std::uint8_t*                         bytes,
        const bluetoe::details::uint128_t&                   value,
        std::uint8_t                                z,
        const bluetoe::details::ecdh_shared_secret_t&        dh_key,
        const bluetoe::details::io_capabilities_t&           io_caps,
        const device_address&                       address )
    {
        { toolbox.generate_keys() }
            -> std::same_as< std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > >;

        { toolbox.select_random_nonce() }
            -> std::same_as< bluetoe::details::uint128_t >;

        { toolbox.p256( bytes, bytes ) }
            -> std::same_as< bluetoe::details::ecdh_shared_secret_t >;

        { toolbox.f4( bytes, bytes, value, z ) }
            -> std::same_as< bluetoe::details::uint128_t >;

        { toolbox.f5( dh_key, value, value, address, address ) }
            -> std::same_as< std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > >;

        { toolbox.f6( value, value, value, value, io_caps, address, address ) }
            -> std::same_as< bluetoe::details::uint128_t >;

        { toolbox.g2( bytes, bytes, value, value ) }
            -> std::same_as< std::uint32_t >;
    };

    /**
     * @brief the constants that describe what a scheduled radio supports
     *
     * Context: any.
     */
    template < typename T >
    concept scheduled_radio_features = requires
    {
        /*
         * Support for link encryption. A radio that supports pairing should also
         * support encryption.
         */
        { T::hardware_supports_encryption } -> std::convertible_to< bool >;

        /*
         * Support for LE Secure Connections pairing. If true, the radio satisfies
         * lesc_pairing_toolbox.
         */
        { T::hardware_supports_lesc_pairing } -> std::convertible_to< bool >;

        /*
         * Support for legacy pairing. The functions this requires are not stated yet.
         */
        { T::hardware_supports_legacy_pairing } -> std::convertible_to< bool >;

        /*
         * Support for the 2 Mbit PHY.
         */
        { T::hardware_supports_2mbit } -> std::convertible_to< bool >;

        /*
         * Support for the user timer being synchronised with connection events.
         */
        { T::hardware_supports_synchronized_user_timer } -> std::convertible_to< bool >;

        /*
         * The radio can provide a link layer context of its own. If true, the link layer
         * may ask for the callbacks to be delivered from a context the radio provides,
         * below the radio's priority and above the application's, instead of from run().
         * Asking for it is a compile time option of the link layer; asking a radio that
         * does not support it is rejected by a static_assert.
         */
        { T::hardware_supports_link_layer_context } -> std::convertible_to< bool >;

        /*
         * Bytes the hardware needs in a package in addition to the PDU: preamble, CRC
         * and whatever else it stores alongside.
         */
        { T::radio_package_overhead } -> std::convertible_to< std::size_t >;

        /*
         * Largest payload the radio can transmit or receive in one package.
         */
        { T::radio_max_supported_payload_length } -> std::convertible_to< std::uint32_t >;

        /*
         * Accuracy of the sleep clock, in parts per million. A test derives the drift it
         * tolerates over an interval from this.
         */
        { T::sleep_time_accuracy_ppm } -> std::convertible_to< std::uint32_t >;
    };

    /**
     * @brief a radio hardware combined with a timer, as a link layer needs it
     *
     * A schedule radio is a template over the type it delivers its callbacks to, so that it can
     * call them without indirection, and reaches that type through the base class
     * relation. The concept checks the pair: CallBacks provides what the radio delivers,
     * and Radio, instantiated with it, provides everything below. Neither side can check
     * the other in its own declaration, since the radio sees its parameter incomplete
     * when it is instantiated as a base class, and the callbacks type cannot name itself
     * in a constraint; the consumer that owns the pair checks it.
     *
     * Every function is annotated with the context it is called from; see the file
     * comment. Every scheduling function is associated with one or more callbacks: once
     * it was called, an action is pending on the radio until one of them is delivered,
     * and as long as an action is pending no other scheduling function is called.
     */
    template < template < typename > class Radio, typename CallBacks >
    concept scheduled_radio =
           scheduled_radio_callbacks< CallBacks >
        && scheduled_radio_features< Radio< CallBacks > >
        && ( !Radio< CallBacks >::hardware_supports_lesc_pairing || lesc_pairing_toolbox< Radio< CallBacks > > )
        && requires (
            Radio< CallBacks >                      radio,
            std::uint32_t                           value,
            abs_time                                when,
            const write_buffer&                     transmit,
            const read_buffer&                      receive,
            phy_ll_encoding::phy_ll_encoding_t      phy,
            const device_address&                   address,
            typename Radio< CallBacks >::ccm_counter_t& counter )
    {
        /*
         * Execution context.
         *
         * run() sleeps until there is something for the application context to do, then
         * returns. A call to wake_up() since the last return guarantees that it returns;
         * the reverse does not hold, run() may return for reasons of its own that are not
         * specified, so a caller does not conclude from a return that wake_up() was
         * called. In a radio without a link layer context of its own, run() is also where
         * the callbacks are delivered, before it returns or sleeps again. Each layer above
         * forwards to the one below and does its own application context work when the
         * call comes back, so that an application loops over the topmost run() regardless
         * of how many contexts the radio has.
         *
         * Context: application.
         */
        radio.run();

        /*
         * Makes run() return. The link layer context calls it after receiving something
         * the application has to process; an interrupt of the application calls it to get
         * the main loop going. It is the guaranteed way to make run() return, not the
         * only one.
         *
         * Context: any, including interrupts.
         */
        radio.wake_up();

        /*
         * Excludes the link layer context while an instance is alive. For the link
         * layer's own state shared between its two contexts; held briefly, from the
         * application context. In a radio without a link layer context of its own this
         * does nothing.
         *
         * Context: application.
         */
        typename Radio< CallBacks >::lock_guard;
        requires std::default_initializable< typename Radio< CallBacks >::lock_guard >;

        /*
         * Setup. These configure the radio for the next action and are applied by the
         * next scheduling call; a single connection link layer calls them when a value
         * changes, a link layer with several connections before every action.
         *
         * Context: link layer.
         */

        /*
         * The access address and CRC initial value for transmitted and received PDUs.
         * Changed only while no action is pending.
         */
        radio.set_access_address_and_crc_init( value, value );

        /*
         * The CCM counters for receiving and transmitting, part of the nonce, changed
         * by the radio when data was exchanged. The type is the implementation's; its
         * only public requirements are default construction, copy and assignment, and
         * a default constructed counter is zero.
         */
        typename Radio< CallBacks >::ccm_counter_t;
        requires std::default_initializable< typename Radio< CallBacks >::ccm_counter_t >;
        requires std::copyable< typename Radio< CallBacks >::ccm_counter_t >;
        radio.set_ccm_counter( counter, counter );

        /*
         * The PHY for the next connection event, receiving and transmitting.
         */
        radio.set_phy( phy, phy );

        /*
         * The local address used for advertising.
         */
        radio.set_local_address( address );

        /*
         * Scheduling.
         *
         * Context: link layer, except start_advertising().
         */

        /*
         * Begins a sequence of advertising events with one as soon as possible.
         * Schedules exactly one advertising event, like schedule_advertising_event(),
         * with the transmission placed at the earliest time the implementation can
         * manage. It does not keep advertising: the caller continues the sequence by
         * scheduling the next event from the callback that ends this one, which carries
         * the time to compute it from.
         *
         * This is how a sequence starts when the caller holds no usable time: the first
         * event after radio_ready(), and every return to advertising after a connection
         * ended or after advertising was switched on while the radio was idle.
         *
         * Context: application or link layer. Switching advertising on happens in the
         * application context while the radio is idle, and it is the radio's job to make
         * that safe against a callback in flight, not the caller's.
         *
         * Returns true if the event was scheduled, false only if another action is
         * pending.
         */
        { radio.start_advertising( value, transmit, transmit, receive ) } -> std::same_as< bool >;

        /*
         * Schedules one advertising event: transmit `transmit` on `channel` so that its
         * first bit is on air at `when`, then listen for a response. A response within
         * the window is delivered with adv_received() in `receive`, which has room for at
         * least two bytes; none is delivered with adv_timeout(). A scan request that
         * passes the white list is answered with the second buffer.
         *
         * Returns true if the event was scheduled, false if `when` was already too close
         * or gone by.
         */
        { radio.schedule_advertising_event( value, when, transmit, transmit, receive ) } -> std::same_as< bool >;

        /*
         * Schedules one connection event: listen on `channel` from `start`, receive PDUs
         * and answer them with pending ones from the buffer that link_layer_pdu_buffer()
         * returns, until the event closes or `end` is reached without any reception. The
         * event is reported with connection_end_event() carrying its start time, or with
         * connection_timeout() carrying `end`. If it was cancelled, nothing is reported.
         *
         * The access address, the CCM counters if the connection is encrypted, and the
         * PHY have been set for this connection, and link_layer_pdu_buffer() returns its
         * buffer.
         *
         * Returns true if the event was scheduled, false if `start` was already too
         * close or gone by.
         */
        { radio.schedule_connection_event( value, when, when ) } -> std::same_as< bool >;

        /*
         * Cancels the pending connection event. The answer is definitive at the time of
         * the call: true, and none of the event's callbacks will be called; false, no
         * event was pending or it was too late, and the event proceeds as if this had
         * not been called. An implementation decides this atomically against the start
         * of the event, so that a caller never sees true and a callback for the same
         * event.
         */
        { radio.cancel_radio_event() } -> std::same_as< bool >;

        /*
         * Timer. At most one is scheduled at any time.
         *
         * Context: link layer.
         */

        /*
         * Schedules user_timer() for `when`, carrying `when`. Returns false, and
         * schedules nothing, if `when` is already gone by. Once delivered, no timer is
         * scheduled.
         */
        { radio.schedule_timer( when ) } -> std::same_as< bool >;

        /*
         * Cancels the timer. True, and user_timer() will not be called for it; false if
         * none was scheduled or it already expired. Definitive, like cancel_radio_event().
         */
        { radio.cancel_timer() } -> std::same_as< bool >;
    };
}
}

#endif
