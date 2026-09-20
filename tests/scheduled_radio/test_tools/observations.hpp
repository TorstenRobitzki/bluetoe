#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_OBSERVATIONS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_OBSERVATIONS_HPP

/**
 * @file observations.hpp
 *
 * What the timing tests build their PDUs from and measure with: advertising channel PDUs,
 * the tolerance of an observed interval and the intervals themselves.
 */

#include "test_tools/rig_fixture.hpp"

#include "host/tester_time.hpp"
#include "link/program.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief how far an observed interval may be off the requested one: the placement of both
     *        events and the drift of two stock crystals over an interval
     */
    constexpr std::chrono::microseconds tolerance{ 50 };

    /**
     * @brief advertising channel PDU types
     * @{
     */
    constexpr std::uint8_t adv_ind         = 0x00;
    constexpr std::uint8_t adv_direct_ind  = 0x01;
    constexpr std::uint8_t adv_nonconn_ind = 0x02;
    constexpr std::uint8_t scan_rsp        = 0x04;
    constexpr std::uint8_t adv_scan_ind    = 0x06;
    /** @} */

    /**
     * @brief an advertising channel PDU of `type` with a payload of `payload_size` bytes: the
     *        advertiser's address, then `fill`
     *
     * The header's TxAdd bit follows the advertiser's address kind.
     */
    std::vector< std::uint8_t > advertising(
        std::size_t payload_size, std::uint8_t fill, std::uint8_t type = adv_nonconn_ind,
        const link_layer::device_address& advertiser = dut_address );

    /**
     * @brief a SCAN_REQ from `scanner` to `advertiser`, as the tester transmits it
     *
     * Header type 0x03, TxAdd the scanner's address kind and RxAdd the advertiser's, then
     * the two addresses in that order, each six bytes as the address stores them.
     */
    std::array< std::uint8_t, 14 > scan_request(
        const link_layer::device_address& scanner, const link_layer::device_address& advertiser );

    /**
     * @brief the time from the first bit of `earlier` to the first bit of `later`, by the
     *        tester's clock
     */
    std::chrono::microseconds time_between( const captured_pdu& earlier, const captured_pdu& later );

    /**
     * @brief the time from the end of `earlier` to the first bit of `later`, by the tester's
     *        clock
     *
     * How long `earlier` was on air follows from `phy`: preamble, access address, the PDU and
     * its CRC, 8 µs a byte at 1 Mbit and 4 µs at 2 Mbit.
     */
    tester_duration inter_frame_space( const captured_pdu& earlier, const captured_pdu& later,
        link_layer::phy_ll_encoding::phy_ll_encoding_t phy = link_layer::phy_ll_encoding::le_1m_phy );

    /**
     * @brief the time from `earlier` to `later`, by the device's clock
     */
    std::chrono::microseconds time_between( const record& earlier, const record& later );
}
}

#endif
