#ifndef BLUETOE_LINK_LAYER_CONNECTION_EVENT_CALLBACK_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_EVENT_CALLBACK_HPP

#include <bluetoe/ll_meta_types.hpp>
#include <bluetoe/delta_time.hpp>

namespace bluetoe {
namespace link_layer {

    namespace details {
        struct connection_event_callback_meta_type : details::valid_link_layer_option_meta_type {};
        struct synchronized_connection_event_callback_meta_type : details::valid_link_layer_option_meta_type {};
        struct check_synchronized_connection_event_callback_meta_type : details::valid_link_layer_option_meta_type {};

        struct default_connection_event_callback
        {
            static void call_connection_event_callback( const delta_time& )
            {
            }

            typedef connection_event_callback_meta_type meta_type;
        };
    }

    /**
     * @brief install a callback that will be called, when a connection event happened.
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
     * @tparam T has to be of class type and fulfil this set of requirements:
     *
     * - A public, default constructible type `connection`
     * - A public, none static member function:
     *
     *   unsigned ll_synchronized_callback( unsigned instant, connection& )
     *
     * - Optional, T contains the following public, none static member functions:
     *
     *   connection ll_synchronized_callback_connect(
     *                  bluetoe::link_layer::delta_time connection_interval,
     *                  unsigned                        calls_per_interval )
     *
     *   void ll_synchronized_callback_period_update(
     *                  bluetoe::link_layer::delta_time connection_interval,
     *                  unsigned                        calls_per_interval,
     *                  connection& )
     *
     *   void ll_synchronized_callback_disconnect( connection& )
     *
     * @tparam Obj A reference to an instance of T on which the specified functions will be called.
     *
     * @tparam MaximumPeriodUS The maximum period at which the callback shall be called, given in µs.
     *
     *         If MaximumPeriodUS is larger than the connections interval, the callback will be called
     *         with a period that is the greatest multiple of the connection interval, that is smaller
     *         than the connections interval.
     *
     *         If MaximumPeriodUS is smaller than the current connections
     *         interval, the callback will be called multiple times with a fixed period. The period is
     *         chosen to be smaller than or equal to the given MaximumPeriodUS and so that a whole number
     *         of calls fit into the connection interval.
     *
     *         Example: Connection Interval = 22.5ms, MaximumPeriodUS = 4ms
     *                      => Effective Period = 3.75ms (callback will be call 6 times)
     *
     *         Example: Connection Interval = 22.5ms, MaximumPeriodUS = 60ms
     *                      => Effective Period = 45ms (callback will be call every second connection event)

     *         If the connections interval changes, the period at which the callback is called also
     *         changes. The callback will be called irrespectively of the connection event. Even when the
     *         connect event times out or is planned to not happen, due to peripheral latency applied.
     *
     * @tparam PhaseShiftUS The offset to the connection anchor in µs. A positive value denotes that
     *         the call has to be called PhaseShiftUS µs after the anchor. Note, that in this case,
     *         high priority CPU processing near the connection event might interrupt the given callback
     *         invocation. A negative value denotes a call to the callback prior to the connection anchor
     *         by the given number of µs.
     *
     * @tparam MaximumExecutionTimeUS The estimated, maximum execution time of the callback. This helps
     *         the library to make sure, that the callback invocation in front of the connection event
     *         will return, before the connection event happens. If the hardware abstraction provides
     *         information about a minimum setup time, the library take that also into account.
     *
     *         If PhaseShiftUS is negative, a compile time check tests that the execution of the callback
     *         will not run into the setup of the connection event.
     *
     * Once a new connection is established, Obj.ll_synchronized_callback_connect() is called (if given)
     * with the connection interval and the number of times, the callback will be called during one interval.
     * ll_synchronized_callback_connect() have to return the initial value of `connection`.
     *
     * If the connection is closed, Obj.ll_synchronized_callback_disconnect() is called with the instance of
     * `connection` that was created by ll_synchronized_callback_connect(). If T does not provide
     * ll_synchronized_callback_connect(), a `connection` is default constructed.
     *
     * If the connection interval changes, Obj.ll_synchronized_callback_period_update() is called (if given)
     * with the connection interval and the number of times, the callback will be called during one interval.
     *
     * ll_synchronized_callback() will be call with the first parameter being 0 for the first invocation
     * after the connection event and then with increased values until the value calls_per_interval - 1
     * is reached (calls_per_interval can be obtained by having ll_synchronized_callback_connect() and
     * ll_synchronized_callback_period_update() defined). The return value of ll_synchronized_callback()
     * denotes the number of times, the invocation of the callback should be omitted. For example, when
     * ll_synchronized_callback() returns 2, two planned callback invocations are omitted until the callback
     * is called again.
     *
     * If the anchor is going to move due to a planned connection update procedure, there is danger
     * of overlapping calls to the callback due to the uncertainty given by the range of transmission
     * window size and offset. In this case, the last callback call before the instant of the update will
     * be omitted. And the next time, the callback will be called, is after the first connection event
     * with updated connection parameters toke place.
     *
     * @note This feature requires support by the hardware abstraction and thus might not be available
     * on all platforms.
     *
     * @note due to connection parameter updates, there is no guaranty, that the callback will be
     *       called with the requested MaximumPeriodUS.
     *
     * @note All callbacks will be called from the very same execution context (thus there is no need
     * for synchronization between calls). The context is defined by the hardware and unspecified.
     */
    template < typename T, T& Obj, unsigned MaximumPeriodUS, int PhaseShiftUS, unsigned MaximumExecutionTimeUS = 100 >
    struct synchronized_connection_event_callback
    {
        /**
         * @brief stop the call of the synchronized callbacks.
         */
        void stop_synchronized_connection_event_callbacks();

        /**
         * @brief restart the invocation of synchronized callbacks, after they where stopped.
         *
         * @pre stop_synchronized_connection_event_callbacks()
         */
        void restart_synchronized_connection_event_callbacks();

        /**
         * @brief Ask Bluetoe to ignore the last return value of ll_synchronized_callback() and call the
         *        callback at the next possible time.
         */
        void force_synchronized_connection_event_callback();

        /** @cond HIDDEN_SYMBOLS */
        template < typename LinkLayer >
        struct impl {
            impl()
                : stopped_( false )
                , force_( false )
            {
                static_assert( LinkLayer::hardware_supports_synchronized_user_timer, "choosen binding does not support the use of user timer!" );
            }

            void stop_synchronized_connection_event_callbacks()
            {
                stopped_ = true;
                force_   = false;

                link_layer().cancel_synchronized_user_timer();
            }

            void restart_synchronized_connection_event_callbacks()
            {
                instance_         = 0;
                latency_          = 0;

                stopped_ = false;
                link_layer().restart_user_timer();
            }

            void force_synchronized_connection_event_callback()
            {
                force_ = true;
            }

            void synchronized_connection_event_callback_new_connection( delta_time connection_interval )
            {
                if ( stopped_ )
                    return;

                connection_value_ = typename T::connection();
                instance_         = 0;
                latency_          = 0;

                calculate_effective_period( connection_interval );
                call_ll_connect< T >( connection_interval, connection_value_ );
                link_layer().cancel_synchronized_user_timer();
                setup_timer( first_timeout() );
            }

            void synchronized_connection_event_callback_start_changing_connection()
            {
                link_layer().cancel_synchronized_user_timer();
            }

            void synchronized_connection_event_callback_connection_changed( delta_time connection_interval )
            {
                if ( stopped_ )
                    return;

                instance_         = 0;
                latency_          = 0;

                calculate_effective_period( connection_interval );
                call_ll_update< T >( connection_interval );
                setup_timer( first_timeout() );
            }

            void synchronized_connection_event_callback_disconnect()
            {
                call_ll_disconnect< T >( 0 );
                link_layer().cancel_synchronized_user_timer();
            }

            void synchronized_connection_event_callback_timeout()
            {
                if ( stopped_ )
                    return;

                if ( force_ )
                {
                    force_   = false;
                    latency_ = 0;
                }

                latency_ = latency_ == 0
                    ? Obj.ll_synchronized_callback( instance_, connection_value_ )
                    : latency_ - 1;

                if ( num_calls_ )
                    instance_ = ( instance_ + 1 ) % num_calls_;

                setup_timer( effective_period_ );
            }

        private:
            template < class TT >
            auto call_ll_connect( bluetoe::link_layer::delta_time connection_interval, typename TT::connection& con )
                -> decltype(&TT::ll_synchronized_callback_connect)
            {
                con = Obj.ll_synchronized_callback_connect( connection_interval, num_calls_ );

                return 0;
            }

            template < class TT >
            void call_ll_connect( ... )
            {
            }

            template < class TT >
            auto call_ll_disconnect( int ) -> decltype(&TT::ll_synchronized_callback_disconnect)
            {
                Obj.ll_synchronized_callback_disconnect( connection_value_ );
                return 0;
            }

            template < class TT >
            void call_ll_disconnect( ... )
            {
            }

            template < class TT >
            auto call_ll_update(
                bluetoe::link_layer::delta_time connection_interval ) -> decltype(&TT::ll_synchronized_callback_period_update)
            {
                Obj.ll_synchronized_callback_period_update( connection_interval, num_calls_, connection_value_ );

                return 0;
            }

            template < class TT >
            void call_ll_update( ... )
            {
            }

            void setup_timer( delta_time dt )
            {
                const bool setup = link_layer().schedule_synchronized_user_timer( dt, delta_time( MaximumExecutionTimeUS ) );
                static_cast< void >( setup );
                assert( setup );
            }

            LinkLayer& link_layer()
            {
                return *static_cast< LinkLayer* >( this );
            }

            void calculate_effective_period( delta_time connection_interval )
            {
                const auto interval_us = connection_interval.usec();

                if ( interval_us > MaximumPeriodUS )
                {
                    num_calls_         = ( interval_us + MaximumPeriodUS - 1 ) / MaximumPeriodUS;
                    num_intervals_     = 0;
                    effective_period_  = delta_time( interval_us / num_calls_ );
                    assert( interval_us % num_calls_ == 0 );
                }
                else
                {
                    num_calls_        = 0;
                    num_intervals_    = MaximumPeriodUS / interval_us;
                    effective_period_ = delta_time( num_intervals_ * interval_us );
                }

                assert( effective_period_.usec() <= MaximumPeriodUS );
            }

            delta_time first_timeout() const
            {
                return PhaseShiftUS < 0
                    ? effective_period_ - delta_time::usec( -PhaseShiftUS )
                    : effective_period_ + delta_time::usec( PhaseShiftUS );
            }

            delta_time effective_period_;

            unsigned instance_;
            unsigned latency_;

            // > 1 if there are more than 1 call to the CB at each interval
            unsigned num_calls_;

            // > 1 if there are more that 1 interval between two CB calls
            unsigned num_intervals_;

            typename T::connection connection_value_;

            volatile bool stopped_;
            volatile bool force_;
        };

        typedef details::synchronized_connection_event_callback_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief Do not user synchronized connection event callbacks
     *
     * This is the default.
     */
    struct no_synchronized_connection_event_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        template < typename LinkLayer >
        struct impl {
            void synchronized_connection_event_callback_new_connection( delta_time )
            {
            }

            void synchronized_connection_event_callback_start_changing_connection()
            {
            }

            void synchronized_connection_event_callback_connection_changed( delta_time )
            {
            }

            void synchronized_connection_event_callback_timeout()
            {
            }

            void synchronized_connection_event_callback_disconnect()
            {
            }
        };

        typedef details::synchronized_connection_event_callback_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief if this parameter is given to the link layer, some compile time checks are done.
     *
     * The compiliation will fail, if the parameters given to synchronized_connection_event_callback<>
     * fail to provide a garanty, that under all possible connection intervals
     * - MaximumPeriodUS can be garantied with all possible connection intervals
     * - PhaseShiftUS is negative and the end of the callback execution leaves enough time
     *   to start the radio.
     * - MaximumExecutionTimeUS does not exceed any possible connection interval taking PhaseShiftUS
     *   and MaximumPeriodUS into account.
     *
     * @sa synchronized_connection_event_callback
     */
    template < unsigned ExpectedMinimumInterval_US >
    struct ranged_check_synchronized_connection_event_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        template < typename Radio >
        static void check( const no_synchronized_connection_event_callback& )
        {
        }

        template < typename Radio, typename T, T& Obj, unsigned MaximumPeriodUS, int PhaseShiftUS, unsigned MaximumExecutionTimeUS >
        static void check( const synchronized_connection_event_callback< T, Obj, MaximumPeriodUS, PhaseShiftUS, MaximumExecutionTimeUS >& )
        {
            static_assert( MaximumPeriodUS <= ExpectedMinimumInterval_US,
                "To guaranty the timely execution of the callback, the MaximumPeriodUS must be small or equal to the minimum connection interval!" );

            static_assert( PhaseShiftUS < 0,
                "In order to make sure, that the callback is finished when the connection event starts, the callback must start prior to the connection event!" );

            static_assert( -PhaseShiftUS >= Radio::connection_event_setup_time_us,
                "PhaseShiftUS must start before the radios setup time." );

            static_assert( -PhaseShiftUS >= Radio::connection_event_setup_time_us + MaximumExecutionTimeUS,
                "Callback execution must end before setup time of the configured radio." );

            static_assert( MaximumPeriodUS / 2 >= MaximumExecutionTimeUS,
                "In worst case, the effective period of the callback invocation is half the requested period and thus must still be large enough for the expected runtimer of the callback." );

            static_assert( -PhaseShiftUS < MaximumPeriodUS / 2,
                "In worst case, the effective period of the callback invocation is half the requested period and thus must still be small than the phase shift." );
        }

        using meta_type = details::check_synchronized_connection_event_callback_meta_type;
        /** @endcond */
    };

    using check_synchronized_connection_event_callback =
        ranged_check_synchronized_connection_event_callback< 7500u >;

    /**
     * @brief no compiler time parameter check
     *
     * @sa check_synchronized_connection_event_callback
     */
    struct no_check_synchronized_connection_event_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        template < typename Radio, class CBs >
        static void check( const CBs& )
        {
        }

        using meta_type = details::check_synchronized_connection_event_callback_meta_type;
        /** @endcond */
    };

}
}
#endif
