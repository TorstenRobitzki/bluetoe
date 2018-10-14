#ifndef BLUETOE_DEVICE_HPP
#define BLUETOE_DEVICE_HPP

#include "nrf51.hpp"

namespace bluetoe
{
    template < class Server, typename ... Options >
    using device = link_layer::link_layer< Server, nrf51_details::scheduled_radio, Options... >;
}

#endif //BLUETOE_DEVICE_HPP
