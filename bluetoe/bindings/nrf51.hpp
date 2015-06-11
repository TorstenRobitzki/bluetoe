#ifndef BLUETOE_BINDINGS_NRF51_HPP
#define BLUETOE_BINDINGS_NRF51_HPP

#include <bluetoe/link_layer/link_layer.hpp>

namespace bluetoe
{

    namespace nrf51_details
    {
        struct scheduled_radio {};
    }

    template < class Server >
    using nrf51 = link_layer::link_layer< Server, nrf51_details::scheduled_radio >;
}

#endif // include guard
