#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO2_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO2_HPP

#include <cstdint>
#include <bluetoe/buffer.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>
#include <bluetoe/connection_events.hpp>
#include <bluetoe/security_connection_data.hpp>
#include <bluetoe/phy_encodings.hpp>
#include <bluetoe/abs_time.hpp>
#include <bluetoe/radio_properties.hpp>

namespace bluetoe {
namespace link_layer {

/**
 * @brief this class declaration shall define the requirements of the
 *        scheduled_radio2 callbacks
 *
 * Context: link layer, for every callback except link_layer_pdu_buffer(),
 * which is called from the radio context and has to return immediately.
 * The white list check that schedule_advertising_event() refers to is
 * the second radio context callback; it is not declared here yet. See
 * the section on contexts of scheduled_radio2.
 */
struct example_callbacks
{
    /**
     * @brief will be called by the radio, when it is ready to operate
     *
     * The implemented delay can be used to wait without polling for
     * PLLs to ramp up, for collecting entrophy, etc.
     *
     * The scheduled_radio2 shall call this function exactly once.
     * The link_layer shall not call any of the scheduling functions
     * before the radio_ready() callback was called.
     *
     * Carries no time. Nothing has happened on the radio yet, and the
     * link layer's first action is scheduled_radio2::start_advertising(),
     * which needs none. An implementation is therefore not required to
     * run a clock before the radio is used.
     */
    void radio_ready();

    /**
     * @brief will be called from the scheduled_radio2, if the
     *        scheduled_radio2::schedule_advertisment() received a response.
     *
     * @param when the point in time, where the first bit of the advertisment
     *             response was received.
     * @param response filled buffer. This buffer was passed priviously to
     *                 scheduled_radio2::schedule_advertisment().
     *
     * @sa scheduled_radio2::schedule_advertisment()
     */
    void adv_received( abs_time when, const read_buffer& response );

    /**
     * @brief call back that will be called when the central does not respond to an advertising PDU
     *
     * @sa scheduled_radio2::schedule_advertisment()
     */
    void adv_timeout( abs_time now );

    /**
     * @brief call back that will be called when connect event times out
     * @sa scheduled_radio2::schedule_connection_event
     */
    void connection_timeout( abs_time now );

    /**
     * @brief call back that will be called after a connect event was closed.
     * @sa scheduled_radio2::schedule_connection_event
     */
    void connection_end_event( abs_time when, connection_event_events evts );

    /**
     * @brief call back that will be called on an expired user timer.
     *
     * @param when point in time, the timer was scheduled.
     */
    void user_timer( abs_time when );

    /**
     * @brief type to retrieve outgoing PDUs from and to store incomming PDUs to.
     */
    struct link_layer_pdu_buffer_t {};

    /**
     * @brief function to provide access to a PDU buffer to the radio
     *
     * This function is used by the radio to get access to the PDU buffer of the current
     * connection. This function must be called only when a connection event was scheduled
     * and the implementation has to make sure, that the function returns the buffer for
     * the current connection (if multiple connections are supported).
     *
     * Context: radio. Called between two PDUs of a connection event, with the inter frame
     * space to spare, so it returns at once and touches nothing but the buffer. The buffer
     * it returns is read and written from the radio context while the link layer fills
     * and drains it from the link layer context, which the buffer has to be built for.
     */
    link_layer_pdu_buffer_t& link_layer_pdu_buffer();
};

/**
 * @brief Set up functions required by the Security Manager to implement
 *        pairing.
 *
 * Context: any, and reentrant. These are pure computations on their arguments,
 * and long ones: a point multiplication takes hundreds of milliseconds on a
 * small core. Where the security manager runs them is its decision.
 */
class pairing_security_toolbox
{
   /**@{
    * @name LE Secure Connections Pairing
    *
    * This set of function needs to be supported, if scheduled_radio2::hardware_supports_lesc_pairing
    * is true. Otherwise, the function should not even be declared, nor defined.
    */
// TODO CHeck
    /**
     * @brief generate public private key pair for DH
     */
    std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys();

    /**
     * @brief random nonce required for LESC pairing
     */
    bluetoe::details::uint128_t select_random_nonce();

    /**
     * @brief p256() security toolbox function, as specified in the core spec
     */
    bluetoe::details::ecdh_shared_secret_t p256( const std::uint8_t* private_key, const std::uint8_t* public_key );

    /**
     * @brief f4() security toolbox function, as specified in the core spec
     */
    bluetoe::details::uint128_t f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z );

    /**
     * @brief f5() security toolbox function, as specified in the core spec
     */
    std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
        const bluetoe::details::ecdh_shared_secret_t dh_key,
        const bluetoe::details::uint128_t& nonce_central,
        const bluetoe::details::uint128_t& nonce_periperal,
        const bluetoe::link_layer::device_address& addr_controller,
        const bluetoe::link_layer::device_address& addr_peripheral );

    /**
     * @brief f6() security toolbox function, as specified in the core spec
     */
    bluetoe::details::uint128_t f6(
        const bluetoe::details::uint128_t& key,
        const bluetoe::details::uint128_t& n1,
        const bluetoe::details::uint128_t& n2,
        const bluetoe::details::uint128_t& r,
        const bluetoe::details::io_capabilities_t& io_caps,
        const bluetoe::link_layer::device_address& addr_controller,
        const bluetoe::link_layer::device_address& addr_peripheral );

    /**
     * @brief g2() security toolbox function, as specified in the core spec
     */
    std::uint32_t g2(
        const std::uint8_t*                 u,
        const std::uint8_t*                 v,
        const bluetoe::details::uint128_t&  x,
        const bluetoe::details::uint128_t&  y );

   /**@}*/

   /**@{
    * @name Legacy Pairing
    *
    * This set of function needs to be supported, if scheduled_radio2::hardware_supports_legacy_pairing
    * is true. Otherwise, the function should not even be declared, nor defined.
    */
   /**@}*/
    // TODO
};

/**
 * @brief abstraction of a radio hardware combined with a timer
 *
 * This is the documentation of requirements to a peripheral abstraction
 * that can be used by a peripheral based link layer implementation.
 *
 * @section time Time
 *
 * The interface has no function that returns the current time. Every abs_time
 * an implementation hands out is the time of something that happened on the
 * radio, given to the callback that reports it, and every time a caller passes
 * in is derived from one of those. A radio that is idle may have only its low
 * frequency clock running, so a time it produced on demand would either be
 * coarse or force the high frequency clock on; inside such a callback the radio
 * has just produced a timestamp and is known to have a clock.
 *
 * The consequence for a caller is that a sequence of radio actions starts with
 * an action that names no time, start_advertising(), and continues with actions
 * whose times are relative to the callbacks the earlier ones produced. See
 * documentation/scheduled_radio_test_rig.md, decision 15.
 *
 * @section contexts Contexts
 *
 * Three contexts exist, named by who lives in them. The radio context is the
 * implementation's own interrupt context. The link layer context is where the
 * callbacks are delivered and where the scheduling functions are called from.
 * The application context is where run() executes and where the GATT layer
 * delivers its callbacks. Every function and callback of this interface states
 * which of them it belongs to: application, link layer, radio, or any.
 *
 * A radio that does not support hardware_supports_link_layer_context delivers
 * the callbacks from inside run(), so the link layer context and the application
 * context are the same. A radio that does provides the link layer context itself,
 * as an interrupt below the radio's priority, and the callbacks arrive from there
 * while run() only sleeps. The contract is the same in both cases; what changes
 * is whether two of the three names denote one context.
 *
 * The link layer's own state shared between its two contexts, data on its way
 * from the application to the PDU buffer and received data on its way up, is
 * protected with lock_guard. The radio's state is never the caller's concern:
 * every scheduling function is called from one context only, except
 * start_advertising(), which the radio makes safe itself.
 *
 * See documentation/scheduled_radio_test_rig.md, decision 17.
 */
template < typename CallBacks, typename... Options >
class scheduled_radio2 : public CallBacks, public pairing_security_toolbox
{
public:
    /**@{
     * @name Features
     *
     * Constants that describe the existance of supported features.
     */

    /**
     * @brief indication of support for link encryption
     *
     * If that constant is true, the radio support the encryption of a
     * link. If the radio supports pairing, it should also support
     * encryption.
     */
    static constexpr bool hardware_supports_encryption = true;

    /**
     * @brief indicates the support for LESC pairing
     *
     * If that constant is true, the radio supports LE secure pairing
     * and thus have to implement the corresponding functions described
     * in pairing_security_toolbox.
     */
    static constexpr bool hardware_supports_lesc_pairing = true;

    /**
     * @brief indicates the support for legacy pairing
     */
    static constexpr bool hardware_supports_legacy_pairing = true;

    /**
     * @brief indication support for pairing
     */
    static constexpr bool hardware_supports_pairing = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

    /**
     * @brief indicates support for 2Mbit
     *
     * If that constant is true, the radio support the 2Mbit PHY.
     */
    static constexpr bool hardware_supports_2mbit = true;

    /**
     * @brief indicates support for schedule_synchronized_user_timer()
     *
     * @sa scheduled_radio2::
     */
    static constexpr bool hardware_supports_synchronized_user_timer = true;

    /**
     * @brief indicates that the radio can provide a link layer context of its own
     *
     * If true, the link layer may ask for the callbacks to be delivered from a
     * context the radio provides, below the radio's priority and above the
     * application's, instead of from run(). Asking for it is a compile time
     * option of the link layer; asking a radio that does not support it is
     * rejected by a static_assert.
     */
    static constexpr bool hardware_supports_link_layer_context = true;

    /**@}*/

    /**@{
     * @name Execution Context Functions
     *
     * What the application context needs from the radio: a place to sleep, a way
     * to be woken, and a lock against the link layer context.
     */

    /**
     * @brief sleep until there is something for the application context to do
     *
     * A call to wake_up() since the last return guarantees that run() returns. The
     * reverse does not hold: run() may return for reasons of its own that are not
     * specified, so a caller does not conclude from a return that wake_up() was
     * called, and keeps its state where the return finds it. In a radio without a
     * link layer context of its own, run() is also where the callbacks are delivered,
     * before it returns or sleeps again. Each layer above forwards to the one below
     * and does its own application context work when the call comes back, so that
     * an application loops over the topmost run() regardless of how many contexts
     * the radio has.
     *
     * Context: application.
     */
    void run();

    /**
     * @brief make run() return
     *
     * The link layer context calls it after receiving something the application has
     * to process; an interrupt of the application calls it to get the main loop going.
     * It is the guaranteed way to make run() return, not the only one.
     *
     * Context: any, including interrupts.
     */
    void wake_up();

    /**
     * @brief excludes the link layer context while an instance is alive
     *
     * For the link layer's own state shared between its two contexts. Held briefly,
     * from the application context. In a radio without a link layer context of its
     * own this does nothing.
     *
     * Context: application.
     */
    class lock_guard;

    /**@}*/

    /**@{
     * @name Attributes
     *
     * Basic attributes of a radio implementation that might differ between implementations.
     */

    /**
     * @brief a number of bytes that are additional required by the hardware to handle an over the air package/PDU.
     *
     * This number includes every byte that have to stored in a package to meet the requirments of the hardware,
     * that is not part of the received / transmitted PDU (CRC, preamble etc.).
     */
    static constexpr std::size_t radio_package_overhead = 0;


    /**
     * @brief maximum length of a radio package payload supported by the radio
     */
    static constexpr std::uint32_t radio_max_supported_payload_length = 255u;

    /**
     * @brief minimum accuracy of the sleep clock
     */
    static constexpr std::uint32_t sleep_time_accuracy_ppm = 20u;

    /**@}*/

    /**@{
     * @name Radio Setup Functions
     *
     * Functions that configure the radio for the next operation. The API is designed,
     * so that for single connection link layers this functions can be called when the
     * parameters in question are changed. For a link layer that supports multiple
     * connections, the link layer probably have to call this setup functions every
     * time it is going to schedule an action.
     *
     * Context: link layer.
     */

    /**
     * @brief set the access address initial CRC value for transmitted and received PDU
     *
     * The values should be changed, when there is no outstanding scheduled transmission or receiving.
     * The values will be applied with the next call to schedule_advertisment() or schedule_connection_event().
     */
    void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

    /**
     * @brief Counter used for CCM
     *
     * Type to store the CCM receive and transmit counters. Only public requirement is a
     * default c'tor, copy and assignment.
     */
    struct ccm_counter_t {
        // set to zero
        ccm_counter_t();
    };

    /**
     * @brief set the packet counters for receiving and transmitting
     *
     * This counters are required as part of the CCM nonce and will be changed, if data
     * was exchanged in a subsequent connection event.
     */
    void set_ccm_counter( ccm_counter_t& receive_counter, ccm_counter_t& transmit_counter );

    /**
     * @brief set the phy to use for the next connection event
     */
    void set_phy(
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receiving_encoding,
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmiting_c_encoding );

    /**
     * @brief set the local address for advertising
     */
    void set_local_address( const bluetoe::link_layer::device_address& );

    /**@}*/

    /**@{
     * @name Scheduling Radio Functions
     *
     * Functions that schedule a radio action on the scheduled_radio2.
     * Every of this function is associated with one or more callback functions.
     * If one of this functions is called, there is an action pending on the radio,
     * until one of the corresponding callbacks is called.
     *
     * As long as there is a pending radio action on the radio, no other scheduling
     * function shall be called.
     *
     * Context: link layer, except start_advertising().
     */

    /**
     * @brief begin a sequence of advertising events with one as soon as possible
     *
     * Schedules exactly one advertising event, like schedule_advertising_event(), with
     * the transmission placed at the earliest time the implementation can manage. It
     * does not keep advertising: the caller continues the sequence by scheduling the
     * next event from the callback that ends this one, which carries the time to
     * compute it from.
     *
     * This is how a sequence starts when the caller holds no usable time: the first
     * event after radio_ready(), and every return to advertising after a connection
     * ended or after advertising was switched on while the radio was idle.
     *
     * Context: application or link layer. Switching advertising on happens in the
     * application context while the radio is idle, and it is the radio's job to make
     * that safe against a callback in flight, not the caller's.
     *
     * @return True, if the event was scheduled. False only if another action is pending.
     */
    bool start_advertising(
        std::uint32_t                               channel,
        const bluetoe::link_layer::write_buffer&    advertising_data,
        const bluetoe::link_layer::write_buffer&    response_data,
        const bluetoe::link_layer::read_buffer&     receive );

    /**
     * @brief schedule an advertising event
     *
     * The function will return immediately. Depending on whether a response is received
     * or the receiving times out, CallBack::adv_received() or CallBack::adv_timeout()
     * is called.
     *
     * White list filtering is applied by calling CallBack::is_scan_request_in_filter().
     *
     * This function is intended to be used for sending advertising PDUs.
     *
     * @param channel Channel to transmit and to receive on.
     * @param when Point in time, when the first bit of data should be started to be transmitted.
     * @param advertising_data The advertising data to be send out.
     * @param response_data The response data used to reply to a scan request, in case the request was in the white list.
     * @param receive Buffer where the radio will copy the received data, before calling Callback::adv_receive().
     *        This buffer have to have at least room for two bytes.
     *
     * @return True, if it was possible to schedule the advertisment. If the function returns false, it
     *         was already to late to schedule the event.
     */
    bool schedule_advertising_event(
        std::uint32_t                               channel,
        abs_time                                    when,
        const bluetoe::link_layer::write_buffer&    advertising_data,
        const bluetoe::link_layer::write_buffer&    response_data,
        const bluetoe::link_layer::read_buffer&     receive );

    /**
     * @brief schedule a connection event
     *
     * The function will return immediately. The function handles a connection event by receiving
     * PDUs and responding with pending PDUs. The function will retriev the outgoing PDUs by a call
     * to CallBacks::link_layer_pdu_buffer() and will post received PDUs by a call to the very same
     * function.
     *
     * If the connection event toke place, CallBacks::connection_end_event() is called with the time,
     * the connection event started and some details about the connection event. If the connection event
     * timed out, CallBacks::connection_timeout() will be called with `end`. If the connection event
     * was canceled by cancel_radio_event(), no callback is called.
     *
     * @pre CallBacks::link_layer_pdu_buffer() has to return a valid buffer for the handled connection.
     * @pre set_access_address_and_crc_init() has to be called with the value associated with the current connection.
     * @pre set_ccm_counter() has to be called if connection is in a encrypted connection to set the counters for
     *                        transmission and reception
     * @pre set_phy() has to be called if connections with different phys are supported
     *
     * @param channel Channel to transmit and to receive on.
     * @param start   Point in time, when the radio start waiting for incomming PDUs.
     * @param end     Point in time, where the radio is turned of and the event considered to be timed out, if no PDU is
     *                received.
     * @return True, if it was possible to schedule the connection event. If the function returns false, it
     *         was already to late to schedule the event.
     *
     * @sa CallBacks::connection_end_event()
     * @sa CallBacks::connection_timeout()
     * @sa cancel_radio_event()
     */
    bool schedule_connection_event(
        std::uint32_t       channel,
        abs_time            start,
        abs_time            end );

    /**
     * @brief cancel a pending connection event
     *
     * The answer is definitive at the time of the call. If the function returns true, the
     * event is canceled and none of its callbacks will be called. If it returns false, no
     * event was pending or it was already too late to cancel it, and the event proceeds
     * as if this function had not been called.
     *
     * An implementation has to decide this atomically against the start of the event, so
     * that a caller can never see true and a callback for the same event.
     *
     * @sa schedule_connection_event()
     */
    bool cancel_radio_event();

    /**@}*/

    /**@{
     * @name Timer Functions
     *
     * At any time, there is at maximum one timer scheduled.
     *
     * Context: link layer.
     */

    /**
     * @brief Schedule the timer
     *
     * If the given point in time (when) is reached, the callback function
     * user_timer() will be called with the scheduled time (which could be
     * different from the actual time). If `when` is already in the past, when
     * the function is called, the function will return false and no timer is
     * scheduled. If the function returns true, the timer is scheduled and the
     * callback will be called (unless the timer is canceled later).
     *
     * Once the callback fuis called, there is no timer scheduled anymore.
     */
    bool schedule_timer( abs_time when );

    /**
     * @brief Cancel the timer
     *
     * If the timer is currently scheduled, the function returns true and
     * user_timer() will not be called for it. If the timer wasn't scheduled,
     * or has already expired, the function returns false. As with
     * cancel_radio_event(), the answer is definitive.
     *
     * @post the timer is not scheduled
     */
    bool cancel_timer();

    /**@}*/

    /**@{
     * @name Test Functions
     *
     * Functions that are purely used when testing the scheduled radio implementation.
     */

    /**
     * @brief message used by the tester to display a message
     *
     * The message should be simply ignored by an implementation. As the traffic between
     * DUT and tester will be logged, the message will appear there.
     */
    void log_tester_message( const char* message );

    /**
     * @brief log message from the DUT
     *
     * This function will be called periodically by the tester to allow the DUT to show
     * log messages. If the return value is not nullptr, the tester will call the function
     * again. Do not return strings that contain `\n`.
     */
    const char* log_get_dut_message();

    /**
     * @brief reset the device
     *
     * Allow the tester to reset the DUT.
     */
    void reset_dut();

    /**
     * @brief static radio properties as runtime information
     *
     * Context: any.
     */
    bluetoe::link_layer::radio_properties properties() const;

    /**@}*/
};


}
}

#endif

