#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUMMY_PORT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUMMY_PORT_HPP

/**
 * @file dummy_port.hpp
 *
 * A serial port for the host, where the rig has none: what lets the host instantiate an
 * instrument's rig template, see dut_functions.hpp and
 * documentation/scheduled_radio_test_rig.md, decision 20. Nothing in it ever runs; the
 * rig's unit tests derive an instrumented version to observe the rig.
 */

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a port that exists only so that the rig can be instantiated on the host
     *
     * It satisfies serial_port and transports nothing.
     */
    template < typename Buffer, typename Wake >
    class dummy_port
    {
    public:
        dummy_port( Buffer&, Buffer&, Wake& ) {}

        void start() {}
        void transmit_pending() {}
    };
}
}

#endif
