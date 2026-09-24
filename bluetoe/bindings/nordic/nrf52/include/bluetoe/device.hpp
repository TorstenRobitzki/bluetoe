#ifndef BLUETOE_DEVICE_HPP
#define BLUETOE_DEVICE_HPP

#include <bluetoe/radio.hpp>
#include <bluetoe/link_layer.hpp>
#include <bluetoe/meta_tools.hpp>

#include <tuple>

namespace bluetoe
{
    namespace nrf52_details
    {
        /*
         * The radio as the link layer names it: a template over the type the callbacks are
         * delivered to. It encrypts when the server needs it to, and takes the options of
         * nrf.hpp a device was given, the binding's options among the link layer's.
         */
        template < bool Encrypting, typename RadioOptions >
        struct radio_for;

        template < typename... RadioOptions >
        struct radio_for< true, std::tuple< RadioOptions... > >
        {
            template < class CallBacks >
            using type = radio< CallBacks, encrypting, RadioOptions... >;
        };

        template < typename... RadioOptions >
        struct radio_for< false, std::tuple< RadioOptions... > >
        {
            template < class CallBacks >
            using type = radio< CallBacks, RadioOptions... >;
        };

        template < typename Server, typename Radio, typename LinkLayerOptions >
        struct link_layer_for;

        template < typename Server, typename Radio, typename... LinkLayerOptions >
        struct link_layer_for< Server, Radio, std::tuple< LinkLayerOptions... > >
        {
            using type = link_layer::link_layer< Server, Radio::template type, LinkLayerOptions... >;
        };
    }

    /**
     * @brief binding to actual hardware
     *
     * The link layer of the GATT server on the radio of this platform. Options are options
     * of the link layer and of the radio, bluetoe::nrf; each goes where it belongs.
     *
     * @sa bluetoe::radio
     */
    template < class Server, typename ... Options >
    using device = typename nrf52_details::link_layer_for<
        Server,
        nrf52_details::radio_for<
            details::requires_encryption_support_t< Server >::value,
            typename details::find_all_by_meta_type< details::binding_option_meta_type, Options... >::type >,
        typename details::find_all_by_not_meta_type< details::binding_option_meta_type, Options... >::type
    >::type;
}

#endif //BLUETOE_DEVICE_HPP
