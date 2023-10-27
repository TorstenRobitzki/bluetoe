#include "radio_callbacks.hpp"

namespace bluetoe {
namespace link_layer {

    void example_callbacks::radio_ready( abs_time now )
    {
    }

    void example_callbacks::adv_received( abs_time when, const read_buffer& response )
    {
    }

    void example_callbacks::adv_timeout( abs_time now )
    {
    }

    void example_callbacks::connection_timeout( abs_time now )
    {
    }

    void example_callbacks::connection_end_event( abs_time when, connection_event_events evts )
    {
    }

    void example_callbacks::connection_event_canceled( abs_time now )
    {
    }

    void example_callbacks::user_timer( abs_time when )
    {
    }

    void example_callbacks::user_timer_canceled()
    {
    }

}
}