#ifndef BLUETOE_BINDINGS_HCI_DEVICE_HPP
#define BLUETOE_BINDINGS_HCI_DEVICE_HPP

#include <bluetoe/link_layer.hpp>

namespace bluetoe
{
    namespace hci_details {
        struct libsub_transport {};
    }

    template < class Server, typename ... Options >
    using device = bluetoe::hci::link_layer< Server, hci_details::libsub_transport, Options... >;
}

#endif

