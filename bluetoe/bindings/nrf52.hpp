#ifndef BLUETOE_BINDINGS_NRF52_HPP
#define BLUETOE_BINDINGS_NRF52_HPP

#include <bluetoe/bindings/nrf51.hpp>

namespace bluetoe
{
    template < class Server, typename ... Options >
    using nrf52 = link_layer::link_layer< Server, nrf51_details::template scheduled_radio_factory<
        nrf51_details::scheduled_radio_base >::scheduled_radio, Options... >;

}

#endif
