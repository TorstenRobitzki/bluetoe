#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_RADIO_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_RADIO_HPP

/**
 * @file radio.hpp
 *
 * The scheduled radio of this platform, as a consumer names it without naming the platform:
 * bluetoe::radio< CallBacks, Options... >, a template over the type it delivers its callbacks
 * to and over its options, satisfying link_layer::scheduled_radio of bluetoe/scheduled_radio2.hpp.
 * Every binding provides this header under the same name, the way device.hpp provides the
 * link layer of the old radio; the two are never used in one firmware.
 */

#include <bluetoe/nrf52_radio.hpp>

namespace bluetoe
{
    template < typename CallBacks, typename... Options >
    using radio = nrf52_details::radio< CallBacks, Options... >;
}

#endif
