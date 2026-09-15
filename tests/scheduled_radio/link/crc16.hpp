#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_CRC16_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_CRC16_HPP

#include <cstddef>
#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the value a CRC-16/CCITT-FALSE starts from
     */
    constexpr std::uint16_t crc16_initial = 0xffff;

    /**
     * @brief CRC-16/CCITT-FALSE: polynomial 0x1021, initial value crc16_initial, no reflection
     *
     * The check value of "123456789" is 0x29b1. A checksum over several pieces is computed
     * by passing the value so far as `initial` for the next piece.
     */
    std::uint16_t crc16( const std::uint8_t* data, std::size_t size, std::uint16_t initial = crc16_initial );
}
}

#endif
