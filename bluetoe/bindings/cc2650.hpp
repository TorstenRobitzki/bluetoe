#ifndef BLUETOE_BINDINGS_CC2650_HPP
#define BLUETOE_BINDINGS_CC2650_HPP

namespace bluetoe
{
    namespace cc2650_details
    {
    }

    template < class Server, typename ... Options >
    using cc2650 = link_layer::link_layer< Server, cc2650_details::scheduled_radio, Options... >;
}

#endif