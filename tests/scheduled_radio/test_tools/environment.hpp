#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_ENVIRONMENT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_ENVIRONMENT_HPP

/**
 * @file environment.hpp
 *
 * The environment variables the radio tests are configured by, one function each.
 */

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief BLUETOE_DUT, the serial device of the device under test
     *
     * @throws rig_error the variable is not set
     */
    std::string dut_device();

    /**
     * @brief BLUETOE_TESTER, the serial device of the tester, if one is named
     */
    std::optional< std::string > tester_device();

    /**
     * @brief BLUETOE_DUT_TIMEOUT_MS, which bounds one request to either instrument
     *
     * The default covers a point multiplication on a small core.
     */
    std::chrono::milliseconds request_timeout();

    /**
     * @brief BLUETOE_TESTER_MIN_RSSI, the weakest signal the tester keeps, in dBm
     *
     * A negative number of decibels, for example -40. Returned as the tester's limit, a
     * count of decibels below a milliwatt; nothing, and the tester keeps every PDU, if the
     * variable is not set.
     *
     * @throws rig_error the variable is set to something outside a receiver's range
     */
    std::optional< std::uint8_t > tester_rssi_limit();
}
}

#endif
