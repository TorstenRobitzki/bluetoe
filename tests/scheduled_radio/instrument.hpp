#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_HPP

/**
 * @file instrument.hpp
 *
 * Requirements common to both instruments of the scheduled radio test setup, the device
 * under test and the tester. See documentation/scheduled_radio_test_rig.md for the reasoning
 * behind these decisions.
 *
 * Both instruments are driven by a host. The host runs the tests; the instruments only
 * execute what they are told and report what they observed.
 *
 * @section transactions Transactions
 *
 * The link is used half duplex and the host always initiates. An instrument never sends
 * anything the host did not ask for. A function that produces a result is therefore a call
 * followed by a poll for that result, and anything that happens on its own is queued until
 * the host collects it.
 *
 * This is what keeps an instrument from ever waiting on the host: its main loop answers a
 * pending request or lets the implementation under test run, and never blocks on the link.
 *
 * No timing information is carried by the arrival of a message. Every time an instrument
 * reports is a value in a payload, so the latency and the jitter of the link do not enter
 * into any measurement.
 *
 * @section boot Detecting a restart
 *
 * Every response carries boot_counter(). The host compares it with the value it saw last;
 * any increment means the instrument restarted since the previous exchange, whether the
 * host asked for that or the instrument crashed. Results that span a restart are void.
 *
 * @section queues Queues
 *
 * Anything an instrument observes while the host is not listening is queued together with
 * the time it happened. A queue that silently drops entries would turn "it did not happen"
 * into a passing assertion, so loss has to be detectable: every entry carries a sequence
 * number, and the host treats a gap as a failed test rather than as an absence of events.
 */

#include <bluetoe/abs_time.hpp>

#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief requirements that both instruments have to satisfy
     */
    class instrument
    {
    public:
        /**
         * @brief version of the protocol spoken over the link
         *
         * The host refuses to talk to an instrument whose protocol version it does not
         * know, rather than misinterpreting its answers.
         */
        std::uint16_t protocol_version();

        /**
         * @brief name of the implementation behind this instrument
         *
         * For a device under test, this identifies the scheduled radio implementation and
         * the hardware it runs on. Two boards on a desk are otherwise hard to tell apart.
         */
        const char* implementation_name();

        /**
         * @brief identifies the firmware build
         */
        const char* build_identifier();

        /**
         * @brief number of times this instrument started since it was flashed
         *
         * Increments on every reset, however caused. Carried in every response.
         *
         * @sa instrument.hpp, section "Detecting a restart"
         */
        std::uint32_t boot_counter();

        /**
         * @brief the instrument's own notion of time
         *
         * The two instruments have unrelated time domains. They are related by observing
         * one event in both, which the host does by having the device under test transmit
         * at a time it reports, and the tester timestamp its arrival.
         *
         * For the tester this clock is the measurement reference and is expected to be far
         * more accurate than the device under test. For a device under test this is the
         * clock of the scheduled radio implementation itself, which is one of the things
         * being measured.
         */
        link_layer::abs_time time_now();

        /**
         * @brief sequence number of the oldest entry that was dropped, if any
         *
         * Zero if nothing was dropped since the last reset. Latches, so the host cannot
         * miss it by polling at the wrong moment.
         */
        std::uint32_t lost_events();
    };
}
}

#endif
