#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

namespace bluetoe {
namespace link_layer {

    template < class Server, class ScheduledRadio >
    class link_layer
    {
    public:
        void run( Server& ) {}
    };
}
}

#endif
