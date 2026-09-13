#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUMMY_PLATFORM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUMMY_PLATFORM_HPP

/**
 * @file dummy_platform.hpp
 *
 * A tester platform for the host, where the tester has none: what lets the host
 * instantiate the tester's rig template, see tester_functions.hpp and
 * documentation/scheduled_radio_test_rig.md, decision 20. Nothing in it ever runs; the
 * tester's unit tests derive an instrumented version to observe the tester.
 */

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a platform that exists only so that the tester can be instantiated on the host
     *
     * It satisfies tester_platform and does nothing.
     */
    class dummy_platform
    {
    public:
        void reset_device_under_test() {}
        void run() {}
        void wake_up() {}
    };
}
}

#endif
