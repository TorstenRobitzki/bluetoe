#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PDU_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PDU_HPP

/**
 * @file pdu.hpp
 *
 * The PDU as it crosses the wire, shared by the device under test's program and the
 * tester's received PDUs: a legacy advertising PDU, two bytes of header and up to 37 of
 * payload. Data channel PDUs, which are larger, come with the connection work.
 */

#include "link/serialize.hpp"

#include <cstddef>

namespace bluetoe {
namespace test_rig {

    constexpr std::size_t max_advertising_pdu_size = 39;

    using pdu = bytes< max_advertising_pdu_size >;
}
}

#endif
