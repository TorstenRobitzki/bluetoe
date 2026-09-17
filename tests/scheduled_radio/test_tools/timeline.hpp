#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_TIMELINE_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_TIMELINE_HPP

/**
 * @file timeline.hpp
 *
 * What a test expects of the PDUs a tester program captured: the entries in the order they
 * were on air, each with its direction, its bytes and whether its CRC was valid.
 */

#include "link/tester_program.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief one entry a tester's timeline is expected to hold
     *
     * A member without a value is one the test does not require anything of, such as the
     * bytes of a PDU the device composed.
     */
    struct expected_pdu
    {
        std::optional< pdu_direction >                  direction   = {};
        std::optional< bool >                           crc_ok      = {};
        std::optional< std::vector< std::uint8_t > >    data        = {};

        /**
         * @brief the same entry, but received with an invalid CRC
         */
        expected_pdu with_crc_error() const;
    };

    /**
     * @brief an entry of an expected timeline
     * @{
     */
    expected_pdu received( std::span< const std::uint8_t > bytes );

    /**
     * @brief a PDU received with a valid CRC whose bytes the test does not know
     */
    expected_pdu received_anything();

    expected_pdu sent( std::span< const std::uint8_t > bytes );

    /**
     * @brief an entry the test does not look at
     */
    expected_pdu anything();
    /** @} */

    /**
     * @brief one entry as a line: its direction, its CRC and its bytes
     *
     * A field `required` does not name is written as `*`, in both lines, so that the two
     * differ in what the test required and nothing else.
     * @{
     */
    std::string as_text( const captured_pdu& entry );

    std::string as_text( const captured_pdu& entry, const expected_pdu& required );

    std::string as_text( const expected_pdu& expected );

    std::string as_text( const std::vector< captured_pdu >& captured );
    /** @} */

    /**
     * @brief how the entries of `captured` and `expected` differ, one line each, in order
     *
     * Only the entries both hold are compared; the lengths are left to check_captured(),
     * which has to stop a test that would go on to read an entry that is not there.
     */
    std::vector< std::string > differences(
        const std::vector< captured_pdu >& captured, const std::vector< expected_pdu >& expected );

    /**
     * @brief require the timeline `captured` to be exactly `expected`
     *
     * A failure names the position, says what differed and prints the whole timeline with it.
     */
    void check_captured( const std::vector< captured_pdu >& captured, const std::vector< expected_pdu >& expected );
}
}

#endif
