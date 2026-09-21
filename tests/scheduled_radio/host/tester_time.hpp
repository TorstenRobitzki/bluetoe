#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_TESTER_TIME_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_TESTER_TIME_HPP

/**
 * @file tester_time.hpp
 *
 * The conversion between the tester's clock ticks and a time, done on the host and nowhere
 * else: the tester reports a tester_time, a count of its 62.5 ns ticks, and a test
 * reads and writes it as a std::chrono duration rather than as a bare number.
 */

#include "link/tester_program.hpp"

#include <chrono>
#include <cstdint>
#include <ratio>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the tester's clock tick as a std::chrono period, 1/16000000 of a second
     */
    using tester_duration = std::chrono::duration< std::int64_t, std::ratio< 1, tester_ticks_per_second > >;

    /**
     * @brief the time a tester_time denotes, as a std::chrono duration since its clock started
     */
    constexpr tester_duration time_of( tester_time when )
    {
        return tester_duration{ static_cast< std::int64_t >( when.ticks ) };
    }

    /**
     * @brief the tester_time at a given time, for expressing an expectation
     *
     * @code
     * platform.push_received( at( 100ms ), ... );
     * BOOST_CHECK( time_of( received[ 0 ].when ) == 100ms );
     * @endcode
     */
    template < typename Rep, typename Period >
    constexpr tester_time at( std::chrono::duration< Rep, Period > when )
    {
        return { static_cast< std::uint64_t >( std::chrono::duration_cast< tester_duration >( when ).count() ) };
    }
}
}

#endif
