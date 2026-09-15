#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_TESTER_FUNCTIONS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_TESTER_FUNCTIONS_HPP

/**
 * @file tester_functions.hpp
 *
 * The functions of the tester, as the host calls them: the tester's function list,
 * obtained the way dut_functions.hpp obtains the device's, by instantiating the rig
 * template with dummies. See documentation/scheduled_radio_test_rig.md, decision 20.
 */

#include "host/dummy_platform.hpp"
#include "host/dummy_port.hpp"
#include "instrument/tester_rig.hpp"

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the tester, as the host spells its functions: &tester::reset_device_under_test
     */
    using tester = tester_rig< dummy_port, dummy_platform >;

    using tester_functions = tester::functions;
}
}

#endif
