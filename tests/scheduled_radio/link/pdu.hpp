#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PDU_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PDU_HPP

/**
 * @file pdu.hpp
 *
 * The PDU as it crosses the wire, shared by the device under test's program and the
 * tester's received PDUs: two bytes of header and the payload, as large as a radio may put
 * on air. A legacy advertising PDU is the smaller case, which the buffers of advertising
 * are sized for.
 */

#include "link/serialize.hpp"

#include <bluetoe/ll_constants.hpp>

#include <cstddef>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a legacy advertising PDU: the header and the largest advertising payload
     */
    constexpr std::size_t max_advertising_pdu_size = link_layer::pdu_header_size + link_layer::max_advertising_payload_size;

    /**
     * @brief the largest data channel PDU: the header and the largest data payload
     */
    constexpr std::size_t max_data_pdu_size = link_layer::pdu_header_size + link_layer::max_data_payload_size;

    /**
     * @brief the largest PDU of any channel: the header and the length field's largest payload
     */
    constexpr std::size_t max_pdu_size = link_layer::pdu_header_size + link_layer::max_payload_size;

    using pdu = bytes< max_pdu_size >;

    /**
     * @brief a PDU of an advertising channel, for what can hold nothing else
     *
     * A rig keeps many of these, so the difference to a data channel PDU is worth the
     * second type: what only ever carries an advertisement does not reserve the room of
     * one.
     */
    using adv_pdu = bytes< max_advertising_pdu_size >;
}
}

#endif
