#ifndef BLUETOE_LINK_LAYER_PERIPHERAL_LATENCY_HPP
#define BLUETOE_LINK_LAYER_PERIPHERAL_LATENCY_HPP

#include <bluetoe/ll_meta_types.hpp>
#include <bluetoe/connection_events.hpp>

/**
 * @file bluetoe/peripheral_latency.hpp
 *
 * This file provides several options, that allow users to configure Bluetoe's
 * support for peripheral latency.
 * Peripheral latency allows a link layer peripheral to ignore some connection events
 * (do not respond to the central) to conserve power.
 *
 * Each option is a compromise between latency and power consumption.
 *
 * @sa bluetoe::link_layer::peripheral_latency_ignored
 * @sa ...
 */
namespace bluetoe {
namespace link_layer {

    namespace details {
        struct peripheral_latency_meta_type {};
    }

    /**
     * @brief this constant is defined by the core spec
     */
    static constexpr auto       maximum_link_layer_peripheral_latency = 499;

    /**
     * @brief detailed options for the peripheral latency behavior
     *
     * Every option defines a set of events / circumstances under which
     * the link layer should listen at the very next connection event.
     * Regardless of the selected options, the link layer always listens
     * for incoming PDU at each peripheral latency anchor points, which
     * were negotiated as part of the current connection parameters.
     *
     * @note in combination, there are some options that make no sense.
     *       For example, using listen_always with some of the other
     *       listen_* options is pointless and will result in failure to compile.
     */
    enum class peripheral_latency
    {
        /**
         * @brief listen at the very next connection event if bluetoe
         * has an available PDU for sending.
         *
         * This reduces the latency of any available payload. If outgoing
         * payload becomes available while the next connection event was
         * planned to utilize peripheral latency, the library tries to
         * reschedule the next connection event to happen earlier.
         */
        listen_if_pending_transmit_data,

        /**
         * @brief listen if the link layer contains unacknowledged data.
         *
         * If a PDU was sent, but no acknowledgement has been received,
         * the link layer will listen at the next connection event.
         *
         * This might be interesting, if the sending queue is very small because
         * Bluetoe has to keep a copy of a transmitted PDU, as long as that PDU
         * is not acknowledged.
         */
        listen_if_unacknowledged_data,

        /**
         * @brief listen to the next connection event, if the last received PDU was
         *        not empty.
         */
        listen_if_last_received_not_empty,

        /**
         * @brief listen to the next connection event, if the last transmitted PDU was
         *        not empty.
         */
        listen_if_last_transmitted_not_empty,

        /**
         * @brief listen to the next connection event, if the last received PDU had
         *        the more data (MD) flag set.
         *
         * With the MD flag, the central (and the peripheral) can indicate, that there
         * is more data to be sent. Usually this data is then transmitted during the same
         * connection event. If there is no support for handling more data in the same
         * connection event either by the peripheral or by the central, then this option
         * may help reducing the latency of the pending data.
         */
        listen_if_last_received_had_more_data,

        /**
         * @brief listen at all connection events, despite the negotiated
         *        peripheral latency value of the current connection.
         */
        listen_always,

    };

    namespace details {
        template < peripheral_latency >
        struct wrap_latency_option {};

        template < peripheral_latency RequestedFeature, peripheral_latency ... Options >
        struct feature_enabled
        {
            using type = typename bluetoe::details::select_type<
                bluetoe::details::has_option< wrap_latency_option< RequestedFeature >, wrap_latency_option< Options >... >::value,
                std::true_type,
                std::false_type >::type;

            static constexpr bool value = type::value;
        };
    }

    /**
     * @brief defines a peripheral configuration to be used by the link layer
     *
     * The configuration is a set of options from peripheral_latency. Example:
     *
     * @code{.cpp}
     * bluetoe::device<
     *    gatt_server_definition,
     *    bluetoe::link_layer::peripheral_latency_configuration<
     *        bluetoe::link_layer::peripheral_latency::listen_if_unacknowledged_data,
     *        bluetoe::link_layer::peripheral_latency::listen_if_last_received_had_error
     *    >
     * > gatt_server;
     * @endcode
     *
     * @sa bluetoe::link_layer::peripheral_latency
     * @sa bluetoe::link_layer::peripheral_latency_configuration_set
     */
    template < peripheral_latency ... Options >
    struct peripheral_latency_configuration
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief allows the peripheral latency configuration to be changed at runtime
     *        between a given set of configurations.
     *
     * Every given Configurations has to be a vaild peripheral_latency_configuration<>.
     *
     * For example:
     *
     * @code{.cpp}
     * bluetoe::device<
     *    gatt_server_definition,
     *    bluetoe::link_layer::peripheral_latency_configuration_set<
     *        bluetoe::link_layer::peripheral_latency::peripheral_latency_ignored,
     *        bluetoe::link_layer::peripheral_latency::peripheral_latency_strict_plus >
     *    >
     * > gatt_server;
     *
     * void foo()
     * {
     *     gatt_server.change_peripheral_latency<
     *         bluetoe::link_layer::peripheral_latency::peripheral_latency_ignored >();
     * }
     * @endcode
     *
     * @sa bluetoe::link_layer::peripheral_latency_configuration
     */
    template < typename ... Configurations >
    class peripheral_latency_configuration_set
    {
    public:
        /**
         * @brief change to a different peripheral latency configuration
         *
         * NewConfig will be the new configuration. NewConfig has be an
         * element of Configurations...
         */
        template < typename NewConfig >
        void change_peripheral_latency();

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::peripheral_latency_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief Instructs the link layer to ignore peripheral latency
     *
     * The link layer will listen at every connection event and will respond at every
     * connection event.
     *
     * This option will provide lowest latency without any chance to conserve power.
     *
     * @sa bluetoe::link_layer::peripheral_latency_configuration
     */
    using peripheral_latency_ignored = peripheral_latency_configuration<
        peripheral_latency::listen_always
    >;

    /**
     * @brief Configure the link layer to only listen every configured connection event.
     *
     * The link layer will only listen to the central every "peripheral latency" + 1 connection
     * events. If for example, the peripheral latency of the current connection is 4, the link
     * layer will just listen to every 5th connection event.
     *
     * If there is pending, outgoing data, the link layer will start to listen on the next possible
     * connection event to be able to reply with the pending data. Also, if a received PDU has the
     * more data (MD) flag beeing set, the link layer will start listening at the next connection event
     * and transmit the pending data.
     *
     * This option provided lowest power consumption while providing better transmitting latency compared
     * to having a connection interval equal to peripheral latency + 1 * interval.
     *
     * @sa bluetoe::link_layer::peripheral_latency_always_listening
     */
    using peripheral_latency_strict = peripheral_latency_configuration<
        peripheral_latency::listen_if_pending_transmit_data,
        peripheral_latency::listen_if_last_received_had_more_data
    >;

    /**
     * @brief Configure the link layer to just listen every configured connection event
     *        and on the next connection events if the previous connection event contained data
     *        from the central.
     *
     * This option extends the peripheral_latency_strict option by listening
     * on subsequent events, if there was data received at the previous event.
     *
     * This option provided lowest power consumption while providing better transmitting latency compared
     * to having a connection interval equal to peripheral latency + 1 * interval. Compared to
     * peripheral_latency_strict, this might increase the receiving bandwidth.
     *
     * @sa bluetoe::link_layer::peripheral_latency_always_listening
     * @sa bluetoe::link_layer::peripheral_latency_strict
     */
    using peripheral_latency_strict_plus = peripheral_latency_configuration<
        peripheral_latency::listen_if_last_received_not_empty,
        peripheral_latency::listen_if_last_received_had_more_data
    >;

    /**
     * @brief Default peripheral latency configuration
     *
     * This configuration is used by bluetoe::link_layer::link_layer.
     *
     * @sa bluetoe::link_layer::peripheral_latency::listen_if_pending_transmit_data
     * @sa bluetoe::link_layer::peripheral_latency::listen_if_unacknowledged_data
     * @sa bluetoe::link_layer::peripheral_latency::listen_if_last_received_not_empty
     * @sa bluetoe::link_layer::peripheral_latency::listen_if_last_transmitted_not_empty
     * @sa bluetoe::link_layer::peripheral_latency::listen_if_last_received_had_more_data
     */
    using periperal_latency_default_configuration = peripheral_latency_configuration<
        peripheral_latency::listen_if_pending_transmit_data,
        peripheral_latency::listen_if_unacknowledged_data,
        peripheral_latency::listen_if_last_received_not_empty,
        peripheral_latency::listen_if_last_transmitted_not_empty,
        peripheral_latency::listen_if_last_received_had_more_data
    >;

    namespace details {

        /**
         * @brief this class implements the decisions regarding, when the next connection
         *        event is planned, based on the events happend at the last connection event
         *        and the configuration.
         *
         * The class implements this by forwarding some of the required pieces of information
         * to it's base class (strategy pattern).
         */
        template < class Base >
        class connection_state_base
        {
        public:
            /**
             * @brief index into the channel map for the next, planned connection event
             */
            unsigned      current_channel_index() const
            {
                return channel_index_;
            }

            /**
             * @brief index into the channel map for the next, planned connection event
             */
            std::uint16_t connection_event_counter() const
            {
                return event_counter_;
            }

            /**
             * @brief distance between the last connection event that happend and the next,
             *        planned connection event.
             *
             * The time since last event raised linear with the number of timeouts that occured
             * on a connection.
             */
            delta_time    time_since_last_event() const
            {
                return time_since_last_event_;
            }

            /**
             * @brief Plan next connection event after timeout
             */
            void plan_next_connection_event_after_timeout(
                delta_time              connection_interval )
            {
                channel_index_ = ( channel_index_ + 1 ) % channel_map::max_number_of_data_channels;
                ++event_counter_;
                time_since_last_event_ += connection_interval;
            }

            /**
             * @brief Plan next connection event
             */
            void plan_next_connection_event(
                std::uint16_t                       connection_peripheral_latency,
                connection_event_events             last_event_events,
                delta_time                          connection_interval,
                std::pair< bool, std::uint16_t >    pending_instance )
            {
                if ( ( base().template peripheral_latency_feature< peripheral_latency::listen_if_unacknowledged_data >() and last_event_events.unacknowledged_data )
                  or ( base().template peripheral_latency_feature< peripheral_latency::listen_if_last_received_not_empty >() and last_event_events.last_received_not_empty )
                  or ( base().template peripheral_latency_feature< peripheral_latency::listen_if_last_transmitted_not_empty >() and last_event_events.last_transmitted_not_empty )
                  or ( base().template peripheral_latency_feature< peripheral_latency::listen_if_last_received_had_more_data >() and last_event_events.last_received_had_more_data )
                  or ( base().template peripheral_latency_feature< peripheral_latency::listen_if_pending_transmit_data >() and last_event_events.pending_outgoing_data )
                  or ( base().template peripheral_latency_feature< peripheral_latency::listen_always >() )
                  or ( last_event_events.error_occured  )
                   )
                {
                    connection_peripheral_latency = 0;
                }

                ++connection_peripheral_latency;

                if ( pending_instance.first )
                {
                    const std::uint16_t instance_distance = event_counter_ < pending_instance.second
                        ? pending_instance.second - event_counter_
                        : pending_instance.second + ~event_counter_ + 1;

                    if ( instance_distance > 0 )
                        connection_peripheral_latency = std::min( connection_peripheral_latency, instance_distance );
                }

                channel_index_  = ( channel_index_ + connection_peripheral_latency ) % channel_map::max_number_of_data_channels;
                event_counter_ += connection_peripheral_latency;
                time_since_last_event_ = connection_peripheral_latency * connection_interval;

                base().disarmable_connection_state_last_latency( connection_peripheral_latency );
            }

            /**
             * @brief to be called, when pending data to be transmitted is detected
             *
             * If listen_if_pending_transmit_data is given as one of the Options,
             * the passed radio has to implement the disarm_connection_event()
             * function. If the function returns true, the connection event was disarmed
             * and a new connection event can be scheduled again.
             *
             * @pre there is a connection event pending on the radio
             * @ret true: there was enough time to disarm the pending connection event
             */
            template < class Radio >
            bool reschedule_on_pending_data( Radio&, delta_time )
            {
                return false;
            }

            /**
             * @brief connection is just established
             */
            void reset_connection_state()
            {
                channel_index_         = 0;
                event_counter_         = 0;
                time_since_last_event_ = delta_time();
                base().disarmable_connection_state_last_latency( 1 );
            }

            void peripheral_latency_move_connection_event( int count, delta_time connection_iterval )
            {
                assert( count <= 0 );
                assert( -count <= maximum_link_layer_peripheral_latency );

                // to have the left side of the modulo beeing always positiv, the largest necessary multiple of
                // channel_map::max_number_of_data_channels is added. count is bound by the maximum peripheral
                // latency.
                static constexpr unsigned offset = ( maximum_link_layer_peripheral_latency / channel_map::max_number_of_data_channels + 1 ) * channel_map::max_number_of_data_channels;

                channel_index_  = ( channel_index_ + count + offset ) % channel_map::max_number_of_data_channels;
                event_counter_ += count;

                time_since_last_event_ -= -count * connection_iterval;
            }

        private:
            unsigned        channel_index_;
            std::uint16_t   event_counter_;
            delta_time      time_since_last_event_;

            Base& base()
            {
                return *static_cast< Base* >( this );
            }
        };

        template < typename DisarmSupport, typename State >
        class disarmable_connection_state;

        template < typename State >
        class disarmable_connection_state< std::true_type, State >
        {
        public:
            void disarmable_connection_state_last_latency( int l )
            {
                assert( l > 0 );
                last_latency_ = l;
            }

        protected:
            template < class Radio >
            bool reschedule_on_pending_data_impl( Radio& radio, delta_time connection_iterval )
            {
                // Do not try to reschedule an event, that can not be rescheduled or was already
                // rescheduled.
                if ( last_latency_ == 1 )
                    return false;

                const std::pair< bool, bluetoe::link_layer::delta_time > rc = radio.disarm_connection_event();

                if ( rc.first )
                {
                    assert( !connection_iterval.zero() );

                    const unsigned times = std::max( 1u, ( rc.second + connection_iterval - delta_time( 1 ) ) / connection_iterval );

                    // only move the connection event further into direction of the current time
                    const int moved = std::min< int >( times, last_latency_ );
                    static_cast< State* >( this )->peripheral_latency_move_connection_event( moved - last_latency_, connection_iterval );

                    last_latency_ = 1;
                }

                return rc.first;
            }

        private:
            int last_latency_;
        };

        template < typename State >
        class disarmable_connection_state< std::false_type, State >
        {
        public:
            void disarmable_connection_state_last_latency( int ) {}

        protected:
            template < class Radio >
            bool reschedule_on_pending_data_impl( Radio&, delta_time )
            {
                return false;
            }
        };

        /**
         * @brief book keeping for the current / next connection event
         */
        template < typename Options >
        class peripheral_latency_state;

        template < peripheral_latency ... Options >
        class peripheral_latency_state< peripheral_latency_configuration< Options... > >
            : public connection_state_base< peripheral_latency_state< peripheral_latency_configuration< Options... > > >
            , public disarmable_connection_state<
                        typename feature_enabled< peripheral_latency::listen_if_pending_transmit_data, Options... >::type,
                        peripheral_latency_state< peripheral_latency_configuration< Options... > > >
        {
        public:
            static constexpr bool listen_always_given = details::feature_enabled< peripheral_latency::listen_always, Options... >::value;
            static constexpr bool other_parameters_given = sizeof...( Options ) > 1;

            static_assert( !( listen_always_given and other_parameters_given ),
                 "if `listen_always` is given, there is no point in adding addition options" );

            /**
             * @brief to be called, when pending data to be transmitted is detected
             *
             * If listen_if_pending_transmit_data is given as one of the Options,
             * the passed radio has to implement the disarm_connection_event()
             * function. If the function returns true, the connection event was disarmed
             * and a new connection event can be scheduled again.
             *
             * @pre there is a connection event pending on the radio
             * @ret true: there was enough time to disarm the pending connection event
             */
            template < class Radio >
            bool reschedule_on_pending_data( Radio& radio, delta_time connection_iterval )
            {
                return this->reschedule_on_pending_data_impl( radio, connection_iterval );
            }

            template < peripheral_latency RequestedFeature >
            constexpr bool peripheral_latency_feature() const
            {
                return feature_enabled< RequestedFeature, Options... >::type::value;
            }

        };

        template < typename ... Configurations >
        struct listen_if_pending_transmit_data_is_part_of_any_configuration
        {
            using type = std::true_type;
        };

        template < typename ... Configurations >
        class peripheral_latency_state< peripheral_latency_configuration_set< Configurations... > >
            : public connection_state_base< peripheral_latency_state< peripheral_latency_configuration_set< Configurations... > > >
            , public disarmable_connection_state<
                        typename listen_if_pending_transmit_data_is_part_of_any_configuration< Configurations... >::type,
                        peripheral_latency_state< peripheral_latency_configuration_set< Configurations... > > >
        {
        public:
            static_assert( sizeof...(Configurations) > 0, "At least on configuration is required!" );

            /**
             * @brief default configuration, that sets the choosen configuration to the first given.
             */
            peripheral_latency_state()
                : current_configuration_( 0 )
            {
            }

            template < class Radio >
            bool reschedule_on_pending_data( Radio& radio, delta_time connection_iterval )
            {
                return this->reschedule_on_pending_data_impl( radio, connection_iterval );
            }

            template < typename NewConfig >
            void change_peripheral_latency()
            {
                using new_index = bluetoe::details::index_of< NewConfig, Configurations... >;
                static_assert( new_index::value < sizeof...( Configurations ), "given NewConfig is not member of Configurations..." );

                current_configuration_ = new_index::value;
            }

            template < peripheral_latency RequestedFeature >
            bool peripheral_latency_feature() const;

        private:
            template < peripheral_latency RequestedFeature >
            bool runtime_feature() const;

            int current_configuration_;
        };

        template < peripheral_latency RequestedFeature, typename Configuration >
        struct has_feature_enabled_impl;

        template < peripheral_latency RequestedFeature, peripheral_latency ...Options >
        struct has_feature_enabled_impl< RequestedFeature, peripheral_latency_configuration< Options... > >
        {
            static constexpr bool value = feature_enabled< RequestedFeature, Options... >::value;
        };


        template < peripheral_latency RequestedFeature, typename ... Configurations >
        struct is_part_of_all_configurations
        {
            template < typename Configuration >
            using has_feature_enabled = has_feature_enabled_impl< RequestedFeature, Configuration >;

            static constexpr bool value =
                bluetoe::details::count_if<
                    std::tuple< Configurations... >,
                    has_feature_enabled
                >::value == sizeof...( Configurations );
        };

        template < peripheral_latency RequestedFeature, typename ... Configurations >
        struct is_part_of_no_configurations
        {
            template < typename Configuration >
            using has_feature_enabled = has_feature_enabled_impl< RequestedFeature, Configuration >;

            static constexpr bool value =
                bluetoe::details::count_if<
                    std::tuple< Configurations... >,
                    has_feature_enabled
                >::value == 0;
        };

        template < typename ... Configurations >
        template < peripheral_latency RequestedFeature >
        bool peripheral_latency_state< peripheral_latency_configuration_set< Configurations... > >::peripheral_latency_feature() const
        {
            // if the feature is set in all or none of the Configurations, the test can be done at compile time
            // and the compiler has a chance to detect dead code
            if ( is_part_of_all_configurations< RequestedFeature, Configurations... >::value )
                return true;

            if ( is_part_of_no_configurations< RequestedFeature, Configurations... >::value )
                return false;

            return runtime_feature< RequestedFeature >();
        }

        template < peripheral_latency RequestedFeature >
        struct peripheral_latency_feature_checker
        {
            inline explicit peripheral_latency_feature_checker( int config )
                : index( config )
                , result( false )
            {
            }

            template< typename Config >
            void each()
            {
                if ( index == 0 && has_feature_enabled_impl< RequestedFeature, Config >::value )
                    result = true;

                --index;
            }

            int index;
            bool result;
        };

        template < typename ... Configurations >
        template < peripheral_latency RequestedFeature >
        bool peripheral_latency_state< peripheral_latency_configuration_set< Configurations... > >::runtime_feature() const
        {
            peripheral_latency_feature_checker< RequestedFeature > check_feature( current_configuration_ );

            bluetoe::details::for_< Configurations... >::
                template each< peripheral_latency_feature_checker< RequestedFeature >& >( check_feature );

            return check_feature.result;
        }
    }
}
}

#endif
