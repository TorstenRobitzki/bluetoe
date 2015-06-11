#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>

namespace bluetoe {
namespace link_layer {


    /**
     * @brief link layer implementation
     *
     * Implements a binding to a server by implementing a link layout on top of a ScheduleRadio device.
     */
    template < class Server, template < class > class ScheduledRadio >
    class link_layer : public ScheduledRadio< link_layer< Server, ScheduledRadio > >
    {
    public:
        void run( Server& );

        void received( const read_buffer& receive );

        void timeout();
    };

    // implementation
    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::run( Server& )
    {
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::received( const read_buffer& receive )
    {
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::timeout()
    {
    }

}
}

#endif
