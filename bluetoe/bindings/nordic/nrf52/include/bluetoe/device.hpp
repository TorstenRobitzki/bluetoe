#ifndef BLUETOE_DEVICE_HPP
#define BLUETOE_DEVICE_HPP

#include <bluetoe/radio.hpp>
#include <bluetoe/link_layer.hpp>

namespace bluetoe
{
    namespace nrf52_details
    {
        /*
         * The radio as the link layer names it: a template over the type the callbacks are
         * delivered to, without options.
         */
        template < class CallBacks >
        using link_layer_radio = radio< CallBacks >;
    }

    /**
     * @brief binding to actual hardware
     *
     * The link layer of the GATT server on the radio of this platform. Options are options
     * of the link layer.
     *
     * @sa bluetoe::radio
     */
    template < class Server, typename ... Options >
    using device = link_layer::link_layer< Server, nrf52_details::link_layer_radio, Options... >;
}

#endif //BLUETOE_DEVICE_HPP
