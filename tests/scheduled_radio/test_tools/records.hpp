#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RECORDS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RECORDS_HPP

/**
 * @file records.hpp
 *
 * What a test reads from the records of the device under test: the callbacks the radio made,
 * in the order it made them, and the calls a step made with the results they returned.
 */

#include "link/program.hpp"

#include <optional>
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
    /** @} */

    /**
     * @brief a connection_end_event() with the flags it is expected to report
     *
     * A flag not named is expected clear, so connection_end_event{} requires all six clear.
     * link_layer::connection_event_events has constructors, which rule out naming its members.
     */
    struct connection_end_event
    {
        bool unacknowledged_data         = false;
        bool last_received_not_empty     = false;
        bool last_transmitted_not_empty  = false;
        bool last_received_had_more_data = false;
        bool pending_outgoing_data       = false;
        bool error_occured               = false;
    };

    /**
     * @brief one entry of an expected sequence of callbacks
     *
     * Converts from a callback_kind, which requires the kind only, and from a
     * connection_end_event, which requires its flags as well.
     */
    struct expected_callback
    {
        expected_callback( callback_kind expected_kind );

        expected_callback( const connection_end_event& expected_events );

        callback_kind                           kind;
        std::optional< connection_end_event >   events;
    };

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
     * @brief a callback as its name
     */
    std::string as_text( callback_kind kind );

    /**
     * @brief the callbacks a program caused, as a line: each with what `expected` requires of
     *        the entry at its position, separated by commas
     *
     * radio_ready() is left out: the rig reports it once when the radio came up after the
     * reset of the fixture, before a program was loaded.
     */
    std::string as_text( const std::vector< record >& records, const std::vector< expected_callback >& expected );

    /**
     * @brief an expected sequence as the same line
     */
    std::string as_text( const std::vector< expected_callback >& expected );

    /**
     * @brief require the callbacks a program caused to be exactly `expected`
     */
    void check_callbacks( const std::vector< record >& records, const std::vector< expected_callback >& expected );
}
}

#endif
