#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUT_FUNCTIONS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUT_FUNCTIONS_HPP

/**
 * @file dut_functions.hpp
 *
 * The functions of the device under test, as the host calls them. The host needs the
 * rig's function list to talk to a device, and the list is a member of the rig template,
 * so the host instantiates the template with a dummy radio and a dummy port and takes the
 * list from that: an opcode is a position in the list and the signatures carry nothing of
 * the radio, so the list agrees with the one the device was built with. See
 * documentation/scheduled_radio_test_rig.md, decision 20.
 */

#include "host/dummy_port.hpp"
#include "host/dummy_radio.hpp"
#include "instrument/dut_rig.hpp"

namespace bluetoe {
namespace test_rig {

    using dut_functions = dut_rig< dummy_radio, dummy_port >::functions;
}
}

#endif
