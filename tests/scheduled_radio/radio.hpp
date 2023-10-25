#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_HPP

#include <optional>
#include <bluetoe/abs_time.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/phy_encodings.hpp>

namespace radio
{
    std::optional< bluetoe::link_layer::abs_time > receive(
            unsigned channel,
            std::uint32_t access_address,
            bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t enc,
            bluetoe::link_layer::delta_time timeout );
}

#endif
