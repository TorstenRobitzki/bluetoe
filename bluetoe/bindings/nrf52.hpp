#ifndef BLUETOE_BINDINGS_NRF52_HPP
#define BLUETOE_BINDINGS_NRF52_HPP

#include <bluetoe/bindings/nrf51.hpp>

namespace bluetoe
{
    template < class Server, typename ... Options >
    using nrf52 = link_layer::link_layer< Server, nrf51_details::scheduled_radio, Options... >;
}

#endif
