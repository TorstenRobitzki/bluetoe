#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_DUT_RIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_DUT_RIG_HPP

/**
 * @file dut_rig.hpp
 *
 * Requirements of the rig that makes a scheduled radio implementation reachable from the
 * host. Everything in instrument.hpp applies here as well.
 *
 * @section resources Resources
 *
 * The rig owns the link and nothing else. Every other hardware resource stays with the
 * scheduled radio implementation, which keeps the rig from perturbing what it measures.
 * The link is served below the priority the radio uses, and its handling never blocks.
 *
 * The rig has no clock of its own. Every time it reports came in through a callback of the
 * implementation, and every time it passes to the implementation is derived from one of
 * those. Those callbacks are exactly the part of the interface under test; a rig with its
 * own timebase would let a test establish time without ever exercising them.
 *
 * @section programs Programs
 *
 * The host loads a program and starts it; the rig executes it without further help. A
 * program is a sequence of steps, and a step names the callback it waits for and the calls
 * to make when that callback arrives. The calls are those of scheduled_radio2.hpp, with
 * every abs_time parameter given as a delta_time, resolved against the time the triggering
 * callback carried. A step runs inside that callback, so the delay between the callback and
 * the call is microseconds, not a serial round trip.
 *
 * The first step runs on start(), when no time exists yet on the device: radio_ready()
 * carries none, and nothing has happened on the radio. It therefore uses
 * start_advertising(), and the callback that ends that event carries the time the next
 * step is computed from.
 *
 * Steps are consumed in order. A callback that does not match the step being waited for is
 * recorded and otherwise ignored. The program is finished when its last step has run and
 * the implementation has reported the end of whatever that step scheduled, so that a host
 * waiting for program_finished() knows that nothing is still going to happen on air.
 *
 * Rather than restating the interface, the mapping from scheduled_radio2.hpp to actions is:
 *
 * - Parameters are those of the interface, with abs_time replaced by a delta_time that is
 *   added to the time of the triggering callback.
 * - Where the interface takes a buffer of data to transmit, the host supplies the bytes
 *   and the rig owns the memory they are copied into.
 * - Where the interface takes a buffer to receive into, the rig provides it. Its contents
 *   reach the host as part of the callback that reports the reception.
 *
 * @section records What is recorded
 *
 * Two things, sharing one sequence of numbers so that the host can reconstruct the order
 * in which they happened: every callback the implementation made, with the time it carried
 * and its other arguments, and every call a step made, with the resolved time argument and
 * the return value. The host collects both after the program finished.
 *
 * @section reset Reset
 *
 * The rig does not reset itself on request. Reset is a hardware input driven by the tester,
 * because the interesting case is a device that stopped answering. How a given part is wired
 * so that the input actually resets it is that port's business; the requirement is that the
 * device restarts, its state is the state after power on, and it reports a boot counter one
 * higher than before.
 *
 * The reset has to be a reset of the hardware and not a restart of the software, because
 * what the implementation does before it reports itself ready is part of what is measured.
 * */

#include <bluetoe/abs_time.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/connection_events.hpp>

#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the callbacks of scheduled_radio2, as the rig names them
     */
    enum class callback_type {
        radio_ready,
        adv_received,
        adv_timeout,
        connection_timeout,
        connection_end_event,
        user_timer
    };

    /**
     * @brief the calls a step can make, as the rig names them
     */
    enum class call_type {
        set_access_address_and_crc_init,
        set_phy,
        set_local_address,
        start_advertising,
        schedule_advertising_event,
        schedule_connection_event,
        cancel_radio_event,
        schedule_timer,
        cancel_timer
    };

    /**
     * @brief one recorded call from the implementation to its callbacks
     */
    struct recorded_callback
    {
        callback_type           callback;

        /**
         * @brief increases by one per recorded callback or executed call
         *
         * A gap tells the host that the queue overflowed and that the test is void.
         */
        std::uint32_t           sequence_number;

        /**
         * @brief the time the callback was given
         *
         * In the time domain of the device under test. Not meaningful for
         * callback_type::radio_ready, which takes no time.
         */
        link_layer::abs_time    when;

        /**
         * @brief for callback_type::adv_received, the bytes that were received
         */
        const std::uint8_t*     received;
        std::size_t             received_size;

        /**
         * @brief for callback_type::connection_end_event, what the implementation reported
         */
        link_layer::connection_event_events events;
    };

    /**
     * @brief one call a program step made into the implementation
     */
    struct executed_call
    {
        call_type               call;

        /**
         * @brief shares its numbering with recorded_callback::sequence_number
         */
        std::uint32_t           sequence_number;

        /**
         * @brief the abs_time the step resolved and passed, for calls that take one
         *
         * This is the time the test asked for, in the domain of the device under test,
         * and the value a test compares with what the tester observed. Not meaningful for
         * start_advertising().
         */
        link_layer::abs_time    when;

        /**
         * @brief the return value, for calls that have one
         */
        bool                    result;
    };

    /**
     * @{
     * @name Program vocabulary
     *
     * A time argument inside a step is a delta_time and denotes the time the triggering
     * callback carried plus that delta. Data to transmit is given as an array, whose size
     * is deduced.
     *
     * An action is one function of scheduled_radio2, named as in the interface and taking
     * its parameters. A step is `on_start( actions... )` for the first one and
     * `on_<callback>( actions... )` for every other.
     *
     * @code
     * dut.program(
     *     on_start(       start_advertising(          37,                 adv_ind ) ),
     *     on_adv_timeout( schedule_advertising_event( 37, event_interval, adv_ind ) ) );
     * @endcode
     */
    struct action;
    struct step;

    template < std::size_t AdvertisingSize, std::size_t ResponseSize = 0 >
    action start_advertising(
        std::uint32_t                           channel,
        const std::uint8_t                      ( &advertising_data )[ AdvertisingSize ],
        const std::uint8_t                      ( &response_data )[ ResponseSize ] = {} );

    template < std::size_t AdvertisingSize, std::size_t ResponseSize = 0 >
    action schedule_advertising_event(
        std::uint32_t                           channel,
        link_layer::delta_time                  when,
        const std::uint8_t                      ( &advertising_data )[ AdvertisingSize ],
        const std::uint8_t                      ( &response_data )[ ResponseSize ] = {} );

    action schedule_connection_event(
        std::uint32_t                           channel,
        link_layer::delta_time                  start,
        link_layer::delta_time                  end );

    action cancel_radio_event();
    action schedule_timer( link_layer::delta_time when );
    action cancel_timer();

    action set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

    template < typename... Actions > step on_start( Actions... );
    template < typename... Actions > step on_adv_received( Actions... );
    template < typename... Actions > step on_adv_timeout( Actions... );
    template < typename... Actions > step on_connection_timeout( Actions... );
    template < typename... Actions > step on_connection_end_event( Actions... );
    template < typename... Actions > step on_user_timer( Actions... );
    /** @} */

    /**
     * @brief requirements of the rig around a scheduled radio implementation
     */
    class dut_rig
    {
    public:
        /**
         * @brief load a program, replacing any earlier one
         *
         * Only accepted after the implementation reported radio_ready().
         */
        template < typename... Steps >
        void program( Steps... steps );

        /**
         * @brief run the first step of the loaded program
         */
        void start();

        /**
         * @brief hand over the recorded callbacks
         *
         * Returns the calls recorded since the last time the host asked, oldest first, and
         * empties the queue. An empty result means the implementation made no calls, which
         * is what a test asserting that nothing happened is waiting for.
         */
        std::size_t collect_callbacks( recorded_callback* out, std::size_t max_count );

        /**
         * @brief hand over the calls the program made
         *
         * Same semantics as collect_callbacks().
         */
        std::size_t collect_executed_calls( executed_call* out, std::size_t max_count );
    };
}
}

#endif
