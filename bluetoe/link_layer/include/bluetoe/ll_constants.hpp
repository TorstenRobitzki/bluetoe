#ifndef BLUETOE_LINK_LAYER_LL_CONSTANTS_HPP
#define BLUETOE_LINK_LAYER_LL_CONSTANTS_HPP

#include <cstddef>
#include <cstdint>

/**
 * @file ll_constants.hpp
 *
 * Values the Core Specification, Vol 6, Part B, fixes for the link layer, in one place for
 * everything that talks to the air: the link layer, the radio bindings and the test rig.
 */
namespace bluetoe {
namespace link_layer {

    /**
     * @brief the access address of every advertising channel PDU (section 2.1.2)
     */
    constexpr std::uint32_t advertising_access_address = 0x8E89BED6;

    /**
     * @brief the CRC initialisation value of the advertising channels (section 3.1.1)
     */
    constexpr std::uint32_t advertising_crc_init = 0x555555;

    /**
     * @brief the inter frame space: from the end of one PDU to the start of the next one
     *        of an exchange, on both channel types (section 4.1.1)
     */
    constexpr std::uint32_t inter_frame_space_us = 150;

    /**
     * @brief the header of a PDU on either channel type, in bytes
     */
    constexpr std::size_t pdu_header_size = 2;

    /**
     * @brief the largest payload of a legacy advertising channel PDU (section 2.3)
     */
    constexpr std::size_t max_advertising_payload_size = 37;

    /**
     * @brief the largest payload of a data channel PDU (section 2.4)
     */
    constexpr std::size_t max_data_payload_size = 251;

    /**
     * @brief the largest payload the length field of a header can announce, on any channel
     */
    constexpr std::size_t max_payload_size = 255;
}
}

#endif
