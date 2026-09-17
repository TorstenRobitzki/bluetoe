#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RECORDS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RECORDS_HPP

/**
 * @file records.hpp
 *
 * What a test reads from the records of the device under test: the callbacks the radio made,
 * in the order it made them, and the calls a step made with the results they returned.
 */

#include "link/program.hpp"

#include <string>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the callbacks a program waits for, for an expected sequence
     * @{
     */
    constexpr callback_kind adv_received         = callback_kind::adv_received;
    constexpr callback_kind adv_timeout          = callback_kind::adv_timeout;
    constexpr callback_kind user_timer           = callback_kind::user_timer;
    constexpr callback_kind connection_timeout   = callback_kind::connection_timeout;
    constexpr callback_kind connection_end_event = callback_kind::connection_end_event;
    /** @} */

    /**
     * @brief the callbacks of `kind` among a device's records, in the order they were recorded
     */
    std::vector< record > callbacks_of( const std::vector< record >& records, callback_kind kind );

    /**
     * @brief the calls of `kind` among a device's records, in the order they were made
     */
    std::vector< record > calls_of( const std::vector< record >& records, call_kind kind );

    /**
     * @brief the one record `records` holds
     *
     * Requires that there is exactly one. By value, so that it outlives the vector an
     * expression like the_only( calls_of( records, kind ) ) builds.
     */
    record the_only( const std::vector< record >& records );

    /**
     * @brief the callbacks a program caused, in order
     *
     * radio_ready() is left out: the rig reports it once when the radio came up after the
     * reset of the fixture, before a program was loaded.
     */
    std::vector< callback_kind > callbacks( const std::vector< record >& records );

    /**
     * @brief the callbacks as their names, separated by commas
     * @{
     */
    std::string as_text( callback_kind kind );

    std::string as_text( const std::vector< callback_kind >& kinds );
    /** @} */

    /**
     * @brief require the callbacks a program caused to be exactly `expected`
     */
    void check_callbacks( const std::vector< record >& records, const std::vector< callback_kind >& expected );
}
}

#endif
