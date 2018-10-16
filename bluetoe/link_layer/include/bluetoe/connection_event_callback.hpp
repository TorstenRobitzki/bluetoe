#ifndef BLUETOE_LINK_LAYER_CONNECTION_EVENT_CALLBACK_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_EVENT_CALLBACK_HPP

namespace bluetoe {
namespace link_layer {

    namespace details {
       struct connection_event_callback_meta_type {};

        struct default_connection_event_callback
        {
            static void call_connection_event_callback( const delta_time& )
            {
            }

            typedef connection_event_callback_meta_type meta_type;
        };
    }

    /**
     * @brief install a callback that will be called, when a connection event happend.
     *
     * The intended use for this feature are cases, where the CPU is switched off and even the high priority
     * ISR of the scheduled radio will not be served.
     *
     * The parameter T have to be a class type with following none static member function:
     *
     * void ll_connection_event_happend();
     *
     * The template parameter RequiredTimeMS defines the minimum time available until the next connection event will
     * happen. The callback is only called, if the given time is available. If the parameter is 0, the callback will
     * be called on every connection event. The link layer will not use slave latency to increase the time between two
     * connection events to reach the RequiredTimeMS.
     */
    template < typename T, T& Obj, unsigned RequiredTimeMS = 0 >
    struct connection_event_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        static void call_connection_event_callback( const delta_time& time_till_next_event )
        {
            if ( RequiredTimeMS == 0 || time_till_next_event >= delta_time::msec( RequiredTimeMS ) )
                Obj.ll_connection_event_happend();
        }

        typedef details::connection_event_callback_meta_type meta_type;
        /** @endcond */
    };


}
}
#endif
