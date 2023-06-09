#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO2_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO2_HPP

#include <cstdint>
#include <buffer.hpp>
#include <address.hpp>
#include <ll_data_pdu_buffer.hpp>

namespace bluetoe {
namespace link_layer {

/**
 * @brief this class declaration shall define the requirements of the
 *        scheduled_radio2 callbacks
 *
 * All callbacks are expected to be called from the run() function of
 * the scheduled_radio2
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
     */
    void radio_ready( time_point_t now );

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
    void adv_received( time_point_t when, const read_buffer& response );

    /**
     * @brief call back that will be called when the central does not respond to an advertising PDU
     *
     * @sa scheduled_radio2::schedule_advertisment()
     */
    void adv_timeout( time_point_t now );

    /**
     * @brief call back that will be called when connect event times out
     * @sa scheduled_radio2::schedule_connection_event
     */
    void connection_timeout( time_point_t now );

    /**
     * @brief call back that will be called after a connect event was closed.
     * @sa scheduled_radio2::schedule_connection_event
     */
    void connection_end_event( time_point_t when, connection_event_events evts );

    /**
     * @brief call back that will be called, if a connection event was canceled
     * @sa scheduled_radio2::cancel_radio_event()
     */
    void connection_event_canceled();

    /**
     * @brief call back that will be called on an expired user timer.
     *
     * @param when point in time, the timer was scheduled.
     */
    void user_timer( time_point_t when );

    /**
     * @brief call back that will be called, if a connection event was canceled
     * @sa scheduled_radio2::()
     */
    void user_timer_canceled();

};

/**
 * @brief this type describes the required time functions of the
 *        scheduled radio.
 *
 * The time system is self referencing by providing the time_now() function
 * as a denotion of the current time, that can be used as a reference for
 * other, scheduled, radio and timer activities.
 */
class radio_time
{
public:
    /**
     * @brief type that allows to store a point in time with a resolution
     *        of at least 1µs.
     *
     * It is expected that this time overflows once in a while. It should be capable
     * of representing points in time in a range of at least 2007s (4s maximum connection
     * interval times, a peripheral latency of 500 and a total, worst case sleep clock accuracy
     * of 0.1%).
     *
     * This means, that a given point in time has to be advanced by at least 2007s without
     * the representation of the time point overflowing more than once.
     */
    using time_point_t = std::uint32_t;

    /**
     * @brief type that allow to store a time duration with a resolution of at least 1µs.
     *
     * This type must be able to represent value from 0s to at least 2007s.
     */
    using time_duration_t = std::uint32_t;

    /**
     * @brief advances now by margin_us µs
     */
    static constexpr time_point_t time_advance(time_point_t now, time_duration_t margin_us);

    /**
     * @brief compares two time points
     *
     * Compares two points in time for beeing in the relation "first is before second".
     */
    static constexpr bool time_is_before(time_point_t first, time_point_t second);

    /**
     * @brief returns the current time
     */
    static time_point_t time_now();
};

/**
 * @brief Set up functions required by the Security Manager to implement
 *        pairing.
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
 *
 */
template < typename CallBacks, typename Options... >
class scheduled_radio2 : public CallBacks, public radio_time, public pairing_security_toolbox
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
    static constexpr bool hardware_supports_encryption;

    /**
     * @brief indicates the support for LESC pairing
     *
     * If that constant is true, the radio supports LE secure pairing
     * and thus have to implement the corresponding functions described
     * in pairing_security_toolbox.
     */
    static constexpr bool hardware_supports_lesc_pairing;

    /**
     * @brief indicates the support for legacy pairing
     */
    static constexpr bool hardware_supports_legacy_pairing;

    /**
     * @brief indication support for pairing
     */
    static constexpr bool hardware_supports_pairing = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

    /**
     * @brief indication of support for pairing
     *
     * If that constant is true, the radio support the encryption of a
     * link.
     */
    static constexpr bool hardware_supports_encryption;

    /**
     * @brief indicates support for 2Mbit
     *
     * If that constant is true, the radio support the 2Mbit PHY.
     */
    static constexpr bool hardware_supports_2mbit;

    /**
     * @brief indicates support for schedule_synchronized_user_timer()
     *
     * @sa scheduled_radio2::
     */
    static constexpr bool hardware_supports_synchronized_user_timer;

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
    static constexpr std::size_t radio_max_supported_payload_length = 255u;

    /**@}*/

    /**@{
     * @name Radio Setup Functions
     *
     * Functions that configure the radio for the next operation. The API is designed,
     * so that for single connection link layers this functions can be called when the
     * parameters in question are changed. For a link layer that supports multiple
     * connections, the link layer probably have to call this setup functions every
     * time it is going to schedule an action.
     */

    /**
     * @brief set the access address initial CRC value for transmitted and received PDU
     *
     * The values should be changed, when there is no outstanding scheduled transmission or receiving.
     * The values will be applied with the next call to schedule_advertisment() or schedule_connection_event().
     */
    void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

    /**@}*/

    /**@{
     * @name Scheduling Radion Functions
     *
     * Functions that schedule an radion action on the scheduled_radio2.
     * Every of this function is associated with one or more callback functions.
     * If one of this functions is called, there is a action pending on the radio,
     * until one of the corresponding callbacks is called.
     *
     * As long as there is a pending radio action on the radio, no other scheduling
     * function shall be called.
     */

    /**
     * @brief schedule an advertising event
     */
    bool schedule_advertising_event(
        unsigned                                    channel,
        time_point_t                                when,
        const bluetoe::link_layer::write_buffer&    advertising_data,
        const bluetoe::link_layer::write_buffer&    response_data,
        const bluetoe::link_layer::read_buffer&     receive );

    void schedule_connection_event(
        unsigned            channel,
        time_point_t        start,
        time_point_t        end );

    bool cancel_radio_event();
    /**@}*/

    /**@{
     * @name Timer Functions
     *
     * At any time, there is at maximum one timer scheduled.
     */

    /**
     * @brief Schedule the timer
     *
     * If the given point in time (when) is reached, the callback function
     * user_timer() will be called with the scheduled time (which could be
     * different from the actual time). If `when` is already in the past, when
     * the function is called, the function will return false and not timer is
     * scheduled. If the function returns true, the timer is scheduled and the
     * callback will be called (unless the timer is canceled later).
     *
     * Once the callback is called, there is no timer scheduled anymore.
     */
    bool schedule_timer( time_point_t when );

    /**
     * @brief Cancel the timer
     *
     * If the timer is currently scheduled, the function returns true and
     * the user_timer_canceled() will be called. If the timer wasn't scheduled
     * the function will return false.
     *
     * @post the timer is not scheduled
     */
    bool cancel_timer();

    /**@}*/
};


}
}

#endif

