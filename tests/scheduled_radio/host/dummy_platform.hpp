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

#include "instrument/tester_rig.hpp"

#include <array>
#include <cstdint>
#include <optional>

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
        void set_access_address_and_crc_init( std::uint32_t, std::uint32_t ) {}
        void receive( std::uint32_t, link_layer::phy_ll_encoding::phy_ll_encoding_t, std::uint64_t, std::uint32_t ) {}
        void answer( std::uint32_t, link_layer::phy_ll_encoding::phy_ll_encoding_t, std::uint64_t,
            const link_layer::device_address&, const adv_pdu&, std::uint32_t ) {}
        bool connection_event( std::uint32_t, link_layer::phy_ll_encoding::phy_ll_encoding_t, std::uint64_t, std::uint32_t,
            const pdu*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t ) { return false; }
        void stop() {}
        void accept_advertiser( std::uint32_t, const link_layer::device_address& ) {}

        std::optional< tester_event > next_event()
        {
            return std::nullopt;
        }
    };
}
}

#endif
