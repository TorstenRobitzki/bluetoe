#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_TESTER_PLATFORM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_TESTER_PLATFORM_HPP

/**
 * @file platform.hpp
 *
 * What the tester needs from its development kit besides the serial port, as
 * instrument/tester_rig.hpp requires it: the reset line to the device under test, and
 * idling until the port has something.
 *
 * The reset line is one pin of the development kit, wired to the RESET pin of the device
 * under test's kit, with the grounds joined. It is driven open drain: pulled low for the
 * reset, released to high impedance otherwise, so that it never fights the kit's own reset
 * circuitry, which sits on the same net.
 */

namespace bluetoe {
namespace test_rig {

    class platform
    {
    public:
        /**
         * @brief configures the reset pin, released
         */
        platform();

        /**
         * @brief pulls the reset line low for a few milliseconds and releases it
         *
         * Long enough for the filtering on a development kit's reset net; the host polls
         * for the device afterwards, so the exact duration does not matter.
         */
        void reset_device_under_test();

        /**
         * @brief sleeps until an event; the port's interrupt is one
         */
        void run();

        void wake_up();
    };
}
}

#endif
