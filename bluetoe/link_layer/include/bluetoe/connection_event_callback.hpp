#ifndef BLUETOE_LINK_LAYER_CONNECTION_EVENT_CALLBACK_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_EVENT_CALLBACK_HPP

namespace bluetoe {
namespace link_layer {

    namespace details {
       struct connection_event_callback_meta_type : details::valid_link_layer_option_meta_type {};

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
     * be called on every connection event. The link layer will not use peripheral latency to increase the time between two
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

    /**
     * @brief Install a callback that will be called with a maximum period synchronized
     *        to the connection events of established connections.
     *
     * @tparam T has to be of class type and fullfil this set of requirements:
     *
     * - A public, default constructible type `connection`
     * - A public, none static member function:
     *
     *   void ll_synchronized_callback( unsigned instant, connection& )
     *
     * - Optional, T contains the following public, none static memberfunctions:
     *
     *   connection ll_synchronized_callback_connect()
     *   void ll_synchronized_callback_period_update( bluetoe::link_layer::delta_time period, connection& )
     *   void ll_synchronized_callback_disconnect( connection& )
     *
     * @tparam Obj A reference to an instance of T on which the specified functions will be called.
     *
     * @tparam MinimumPeriodUS The minimum period at which the callback shall be called, given in µs.
     *         If MinimumPeriodUS is larger than the connections interval, the callback will be called
     *         once every connection interval. If MinimumPeriodUS is smaller than the current connections
     *         interval, the callback will be called multiple times with a fixed period. The period is
     *         chosen to be smaller than or equal to the given MinimumPeriodUS and so that a whole number
     *         of calls fit into the connection interval.
     *
     *         Example: Connection Interval = 22.5ms, MinimumPeriodUS = 4ms
     *                      => Effective Period = 3.75ms
     *
     *         If the connections interval changes, the period at which the callback is called also
     *         changes. The callback will be called irespectable of the connection event. Even when the
     *         connect event times out or is planned to not happen, due to peripheral latency applied.
     *
     * @tparam PhaseShiftUS The offset to the connection anchor in µs. A possitive value denotes that
     *         the call has to be called PhaseShiftUS µs after the anchor. Note, that in this case,
     *         high priority CPU processing near the connection event might interrupt the given callback
     *         invokation. A negativ value denotes a call to the callback prior to the connection anchor
     *         by the given number of µs.
     *
     * @tparam MaximumExecutionTimeUS The estimated, maximum execution time of the callback. This helps
     *         the library to make sure, that the callback invokation in front of the connection event
     *         will return, before the connection event happens. If the hardware abstration provides
     *         information about a minimum setup time, the library take that also into account.
     *
     *         If PhaseShiftUS is negativ, a compiletime check tests that the execution of the callback
     *         will not run into the setup of the connection event.
     *
     * Once a new connection is established, Obj.ll_synchronized_callback_connect() is called (if given).
     * If the connection is closed, Obj.ll_synchronized_callback_disconnect() is called with the instance of
     * `connection` that was created by ll_synchronized_callback_connect(). If T does not provide
     * ll_synchronized_callback_connect(), a `connection` is default constructed.
     *
     * If the connection interval changes, Obj.ll_synchronized_callback_period_update() is called (if given).
     *
     * If the anchor is going to move due to a planned connection update procedure, there is danger
     * of overlapping calls to the callback due to the uncertency given by the range of transmission
     * window size and offset. In this case, the last callback call before the instant of the update will
     * be ommitted.
     *
     * @note This feature requires support by the hardware abstraction and thus might not be available
     * on all plattforms.
     *
     * @note due to connection parameter updates, there is no guaranty, that the callback will be
     *       called with the requested MinimumPeriodUS.
     */
    template < typename T, T& Obj, unsigned MinimumPeriodUS, int PhaseShiftUS, unsigned MaximumExecutionTimeUS = 0 >
    struct synchronized_connection_event_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::connection_event_callback_meta_type meta_type;
        /** @endcond */
    };
}
}
#endif
