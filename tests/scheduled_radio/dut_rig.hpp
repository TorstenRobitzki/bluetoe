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
 * The rig has no clock of its own. Every time it reports comes from the implementation,
 * through time_now() or through a callback, which are exactly the parts of the interface
 * under test. A rig with its own timebase would let a test establish time without ever
 * exercising them.
 *
 * @section calls Calling the interface
 *
 * Every function of the scheduled radio interface is callable from the host. Rather than
 * restating them here, where a second copy would drift from the first, the mapping is:
 *
 * - Parameters and return values are those of scheduled_radio2.hpp.
 * - Where the interface takes a buffer of data to transmit, the host supplies the bytes
 *   and the rig owns the memory they are copied into.
 * - Where the interface takes a buffer to receive into, the rig provides it. Its contents
 *   reach the host as part of the event that reports the reception.
 * - Functions that return a value return it in the response to the call.
 *
 * @section events Callbacks
 *
 * Callbacks are not delivered to the host as they happen. The implementation calls them on
 * the rig, which records them and hands them over when the host next asks.
 *
 * A recorded callback carries which callback it was, the abs_time it was given, its other
 * arguments, and a sequence number. The order of the queue is the order in which the
 * implementation made the calls.
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
 */

#include <bluetoe/abs_time.hpp>
#include <bluetoe/connection_events.hpp>

#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief one recorded call from the implementation to its callbacks
     */
    struct recorded_callback
    {
        /**
         * @brief which callback the implementation called
         */
        enum class type {
            radio_ready,
            adv_received,
            adv_timeout,
            connection_timeout,
            connection_end_event,
            connection_event_canceled,
            user_timer,
            user_timer_canceled
        };

        type                    callback;

        /**
         * @brief increases by one per recorded callback
         *
         * A gap tells the host that the queue overflowed and that the test is void.
         */
        std::uint32_t           sequence_number;

        /**
         * @brief the time the callback was given
         *
         * In the time domain of the device under test. Not meaningful for
         * type::user_timer_canceled, which takes no time.
         */
        link_layer::abs_time    when;

        /**
         * @brief for type::adv_received, the bytes that were received
         */
        const std::uint8_t*     received;
        std::size_t             received_size;

        /**
         * @brief for type::connection_end_event, what the implementation reported
         */
        link_layer::connection_event_events events;
    };

    /**
     * @brief requirements of the rig around a scheduled radio implementation
     */
    class dut_rig
    {
    public:
        /**
         * @brief hand over the recorded callbacks
         *
         * Returns the calls recorded since the last time the host asked, oldest first, and
         * empties the queue. An empty result means the implementation made no calls, which
         * is what a test asserting that nothing happened is waiting for.
         */
        std::size_t collect_callbacks( recorded_callback* out, std::size_t max_count );

        /**
         * @brief how many callbacks the queue can hold between two collections
         *
         * The host uses this to decide how often it has to poll during a test, and it is
         * part of what an implementation of this rig has to state.
         */
        std::size_t callback_queue_size();
    };
}
}

#endif
