#ifndef BLUETOE_BINDINGS_HCI_DEVICE_HPP
#define BLUETOE_BINDINGS_HCI_DEVICE_HPP

#include <bluetoe/link_layer.hpp>
#include <bluetoe/libsub.hpp>

namespace bluetoe
{
    template < class Server, typename ... Options >
    using device = bluetoe::hci::link_layer< Server, hci_details::libsub_transport, Options... >;
}

#endif

