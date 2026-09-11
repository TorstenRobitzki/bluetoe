#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_ERRORS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_ERRORS_HPP

/**
 * @file errors.hpp
 *
 * Everything that voids a measurement without being a result of it. Tests never check
 * for these; they surface as errors, not as failed expectations, so that a test reads as
 * expectations only.
 */

#include <cstdint>
#include <stdexcept>
#include <string>

namespace bluetoe {
namespace test_rig {

    struct rig_error : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    /**
     * @brief the link did not behave: no response, a corrupt frame, a malformed answer
     */
    struct link_error : rig_error
    {
        using rig_error::rig_error;
    };

    /**
     * @brief a response carried a different session token than expected
     *
     * The instrument restarted since the token was set, or something else answered.
     */
    struct instrument_restarted : rig_error
    {
        instrument_restarted( std::uint32_t expected, std::uint32_t received )
            : rig_error( "expected session token " + std::to_string( expected ) + ", received " + std::to_string( received ) )
            , expected( expected )
            , received( received )
        {
        }

        const std::uint32_t expected;
        const std::uint32_t received;
    };
}
}

#endif
