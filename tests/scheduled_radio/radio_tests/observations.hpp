#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_OBSERVATIONS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_OBSERVATIONS_HPP

/**
 * @file observations.hpp
 *
 * What the timing tests build their PDUs from and read their results with: advertising
 * channel PDUs, and the records and captures of a run.
 */

#include "radio_tests/rig_fixture.hpp"

#include "link/program.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>

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
    constexpr long tolerance_us = 50;

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
     * @brief the callbacks of `kind` among a device's records, in the order they were recorded
     */
    std::vector< record > callbacks_of( const std::vector< record >& records, callback_kind kind );

    /**
     * @brief the calls of `kind` among a device's records, in the order they were made
     */
    std::vector< record > calls_of( const std::vector< record >& records, call_kind kind );

    /**
     * @brief whether a captured PDU consists of exactly `bytes`
     */
    bool carries( const captured_pdu& p, std::span< const std::uint8_t > bytes );

    /**
     * @brief the time from the first bit of `earlier` to the first bit of `later`, in whole
     *        microseconds
     */
    long microseconds_between( const captured_pdu& earlier, const captured_pdu& later );
}
}

#endif
