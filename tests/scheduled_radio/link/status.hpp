#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_STATUS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_STATUS_HPP

/**
 * @file status.hpp
 *
 * The status byte of every response. See documentation/scheduled_radio_test_rig.md,
 * decision 6.
 */

#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief whether the instrument executed the request
     *
     * Only status::ok is followed by a result. Every other value says that the request
     * never reached the function: the instrument checks the request before it calls
     * anything, so a function is either called with exactly the arguments the host sent
     * or not at all.
     */
    enum class status : std::uint8_t
    {
        ok                   = 0,
        unknown_function     = 1,
        malformed_arguments  = 2,

        /*
         * The function is in the list, but this instrument does not implement it: a
         * feature the list has and the device lacks, such as the pairing toolbox of a
         * radio without one.
         */
        unsupported_function = 3
    };

    /**
     * @brief the status in words, for error messages on the host
     */
    const char* describe( status s );
}
}

#endif
