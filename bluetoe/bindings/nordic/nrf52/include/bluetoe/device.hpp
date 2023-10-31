#ifndef BLUETOE_DEVICE_HPP
#define BLUETOE_DEVICE_HPP

#if defined BLUETOE_SCHEDULED_RADIO_2

#include <bluetoe/nrf52_2.hpp>

namespace bluetoe
{
    /**
     * @brief binding to actual hardware
     *
     * @sa nrf52
     */
    // template < typename ... Options >
    // using radio = typename nrf52_details::radio_factory<
    //     details::requires_encryption_support_t< Server >::value,
    //     typename nrf52_details::radio_options< Options... >::result,
    //     typename nrf52_details::link_layer_options< Options... >::result
    // >::radio_t;
}

#else

#include <bluetoe/nrf52.hpp>

namespace bluetoe
{
    /**
     * @brief binding to actual hardware
     *
     * @sa nrf52
     */
    template < class Server, typename ... Options >
    using device = typename nrf52_details::link_layer_factory<
        Server,
        details::requires_encryption_support_t< Server >::value,
        typename nrf52_details::radio_options< Options... >::result,
        typename nrf52_details::link_layer_options< Options... >::result
    >::link_layer;
}
#endif
#endif //BLUETOE_DEVICE_HPP
