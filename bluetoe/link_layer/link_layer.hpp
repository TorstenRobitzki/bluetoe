#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/link_layer/address.hpp>
#include <bluetoe/link_layer/channel_map.hpp>
#include <bluetoe/link_layer/notification_queue.hpp>
#include <bluetoe/link_layer/connection_callbacks.hpp>
#include <bluetoe/link_layer/l2cap_signaling_channel.hpp>
#include <bluetoe/link_layer/white_list.hpp>
#include <bluetoe/link_layer/advertising.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/sm/security_manager.hpp>

#include <algorithm>
#include <cassert>

namespace bluetoe {
    namespace details {
        class notification_data;
    }

namespace link_layer {

    namespace details {
        template < typename ... Options >
        struct buffer_sizes
        {
            typedef typename ::bluetoe::details::find_by_meta_type<
                buffer_sizes_meta_type,
                Options...,
                ::bluetoe::link_layer::buffer_sizes<>  // default
            >::type s_type;

            static constexpr std::size_t tx_size = s_type::transmit_buffer_size;
            static constexpr std::size_t rx_size = s_type::receive_buffer_size;
        };

        template < typename ... Options >
        struct security_manager {
            typedef typename bluetoe::details::find_by_meta_type<
                bluetoe::details::security_manager_meta_type,
                Options...,
                no_security_manager >::type type;
        };

        template < typename ... Options >
        struct signaling_channel {
            typedef typename bluetoe::details::find_by_meta_type<
                bluetoe::details::signaling_channel_meta_type,
                Options...,
                bluetoe::l2cap::no_signaling_channel >::type type;
        };

        template < typename ... Options >
        struct mtu_size {
            typedef typename bluetoe::details::find_by_meta_type<
                mtu_size_meta_type,
                Options...,
                max_mtu_size< 23 > >::type type;

            static constexpr std::size_t mtu = type::mtu;
        };

        template < typename Server, typename ... Options >
        struct connection_callbacks
        {
            typedef typename bluetoe::details::find_by_meta_type<
                connection_callbacks_meta_type,
                Options...,
                no_connection_callbacks >::type callbacks;

            typedef typename callbacks::template impl< Server > type;
        };

        template < typename Radio, typename LinkLayer, typename ... Options >
        struct white_list
        {
            typedef typename bluetoe::details::find_by_meta_type<
                white_list_meta_type,
                Options...,
                no_white_list >::type list;

            typedef typename list::template impl< Radio, LinkLayer > type;
        };
    }

    /**
     * @brief link layer implementation
     *
     * Implements a binding to a server by implementing a link layout on top of a ScheduleRadio device.
     *
     * @sa connectable_undirected_advertising
     * @sa connectable_directed_advertising
     * @sa scannable_undirected_advertising
     * @sa non_connectable_undirected_advertising
     * @sa auto_start_advertising
     * @sa no_auto_start_advertising
     */
    template <
        class Server,
        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBack
        >
        class ScheduledRadio,
        typename ... Options
    >
    class link_layer :
        public ScheduledRadio<
            details::buffer_sizes< Options... >::tx_size,
            details::buffer_sizes< Options... >::rx_size,
            link_layer< Server, ScheduledRadio, Options... >
        >,
        public details::security_manager< Options... >::type,
        public details::white_list<
            ScheduledRadio<
                details::buffer_sizes< Options... >::tx_size,
                details::buffer_sizes< Options... >::rx_size,
                link_layer< Server, ScheduledRadio, Options... >
            >,
            link_layer< Server, ScheduledRadio, Options... >,
            Options... >::type,
        public details::select_advertiser_implementation<
            link_layer< Server, ScheduledRadio, Options... >,
            Options... >,
        private details::connection_callbacks< Server, Options... >::type,
        private details::signaling_channel< Options... >::type
    {
    public:
        link_layer();

        /**
         * @brief this function passes the CPU to the link layer implementation
         *
         * This function should return on certain events to alow user code to do
         * usefull things. Details depend on the ScheduleRadio implemention.
         */
        void run( Server& );

        /**
         * @brief call back that will be called when the master responds to an advertising PDU
         * @sa scheduled_radio::schedule_advertisment_and_receive
         */
        void adv_received( const read_buffer& receive );

        /**
         * @brief call back that will be called when the master does not respond to an advertising PDU
         * @sa scheduled_radio::schedule_advertisment_and_receive
         */
        void adv_timeout();

        /**
         * @brief call back that will be called when connect event times out
         * @sa scheduled_radio::schedule_connection_event
         */
        void timeout();

        /**
         * @brief call back that will be called after a connect event was closed.
         * @sa scheduled_radio::schedule_connection_event
         */
        void end_event();

        /**
         * @brief initiating the change of communication parameters of an established connection
         *
         * If it was not possible to initiate the connection parameter update, the function returns false.
         * @todo Add parameter that identifies the connection.
         */
        bool connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout );

        /**
         * @brief terminates the give connection
         *
         * @todo Add parameter that identifies the connection.
         */
        void disconnect();

        /**
         * @brief fills the given buffer with l2cap advertising payload
         */
        std::size_t fill_l2cap_advertising_data( std::uint8_t* buffer, std::size_t buffer_size ) const;

        /**
         * @brief returns the own local device address
         */
        const device_address& local_address() const;
    private:
        typedef ScheduledRadio<
            details::buffer_sizes< Options... >::tx_size,
            details::buffer_sizes< Options... >::rx_size,
            link_layer< Server, ScheduledRadio, Options... > > radio_t;

        typedef notification_queue<
            Server::number_of_client_configs,
            typename Server::connection_data > notification_queue_t;

        typedef typename details::security_manager< Options... >::type security_manager_t;
        typedef typename details::signaling_channel< Options... >::type signaling_channel_t;

        typedef details::select_advertiser_implementation<
            link_layer< Server, ScheduledRadio, Options... >, Options... > advertising_t;

        unsigned sleep_clock_accuracy( const read_buffer& receive ) const;
        bool check_timing_paremeters( std::uint16_t slave_latency, delta_time timeout ) const;
        bool parse_timing_parameters_from_connect_request( const read_buffer& valid_connect_request );
        bool parse_timing_parameters_from_connection_update_request( const write_buffer& valid_connect_request );
        void force_disconnect();
        void start_advertising_impl();
        void wait_for_connection_event();
        void transmit_notifications();
        void transmit_signaling_channel_output();
        void transmit_pending_control_pdus();

        static bool lcap_notification_callback( const ::bluetoe::details::notification_data& item, void* usr_arg, typename Server::notification_type type );

        enum class ll_result {
            go_ahead,
            disconnect
        };

        ll_result handle_received_data();
        ll_result send_control_pdus();
        ll_result handle_ll_control_data( const write_buffer& pdu, read_buffer output );
        ll_result handle_l2cap( const write_buffer& pdu, const read_buffer& output );
        ll_result handle_pending_ll_control();

        connection_details details() const;

        static std::uint16_t read_16( const std::uint8_t* );
        static std::uint32_t read_24( const std::uint8_t* );
        static std::uint32_t read_32( const std::uint8_t* );

        static constexpr unsigned       first_advertising_channel   = 37;
        static constexpr unsigned       num_windows_til_timeout     = 5;

        static constexpr std::uint8_t   ll_control_pdu_code         = 3;
        static constexpr std::uint8_t   lld_data_pdu_code           = 2;

        static constexpr std::uint8_t   LL_CONNECTION_UPDATE_REQ    = 0x00;
        static constexpr std::uint8_t   LL_CHANNEL_MAP_REQ          = 0x01;
        static constexpr std::uint8_t   LL_TERMINATE_IND            = 0x02;
        static constexpr std::uint8_t   LL_UNKNOWN_RSP              = 0x07;
        static constexpr std::uint8_t   LL_FEATURE_REQ              = 0x08;
        static constexpr std::uint8_t   LL_FEATURE_RSP              = 0x09;
        static constexpr std::uint8_t   LL_VERSION_IND              = 0x0C;
        static constexpr std::uint8_t   LL_CONNECTION_PARAM_REQ     = 0x0F;
        static constexpr std::uint8_t   LL_CONNECTION_PARAM_RSP     = 0x10;
        static constexpr std::uint8_t   LL_PING_REQ                 = 0x12;
        static constexpr std::uint8_t   LL_PING_RSP                 = 0x13;

        static constexpr std::uint8_t   LL_VERSION_NR               = 0x08;
        static constexpr std::uint8_t   LL_VERSION_40               = 0x06;

        static constexpr std::uint16_t  l2cap_att_channel           = 4;
        static constexpr std::uint16_t  l2cap_signaling_channel     = 5;
        static constexpr std::uint16_t  l2cap_sm_channel            = 6;

        static constexpr std::size_t    l2cap_header_size           = 4;
        static constexpr std::size_t    all_header_size             = 6;

        struct link_layer_feature {
            enum : std::uint8_t {
                le_encryption                           = 0x01,
                connection_parameters_request_procedure = 0x02,
                extended_reject_indication              = 0x04,
                slave_initiated_features_exchange       = 0x08,
                le_ping                                 = 0x10,
                le_data_packet_length_extension         = 0x20,
                ll_privacy                              = 0x40,
                extended_scanner_filter_policies        = 0x80
            };
        };

        static constexpr std::uint8_t   supported_features =
            link_layer_feature::connection_parameters_request_procedure |
            link_layer_feature::le_ping;


        // TODO: calculate the actual needed buffer size for advertising, not the maximum
        static_assert( radio_t::size >= advertising_t::maximum_required_advertising_buffer, "buffer to small" );

        const device_address            address_;
        unsigned                        current_channel_index_;
        channel_map                     channels_;
        unsigned                        cumulated_sleep_clock_accuracy_;
        delta_time                      transmit_window_offset_;
        delta_time                      transmit_window_size_;
        delta_time                      connection_interval_;
        std::uint16_t                   slave_latency_;
        std::uint16_t                   timeout_value_;
        delta_time                      connection_interval_old_;
        std::uint16_t                   conn_event_counter_;
        std::uint16_t                   defered_conn_event_counter_;
        write_buffer                    defered_ll_control_pdu_;
        unsigned                        timeouts_til_connection_lost_;
        unsigned                        max_timeouts_til_connection_lost_;
        Server*                         server_;
        notification_queue_t            connection_details_;
        bool                            termination_send_;
        std::uint8_t                    used_features_;

        enum class state
        {
            initial,
            advertising,
            connecting,
            connection_update,
            connected,
            disconnecting
        }                               state_;

        std::uint16_t                   proposed_interval_min_;
        std::uint16_t                   proposed_interval_max_;
        std::uint16_t                   proposed_latency_;
        std::uint16_t                   proposed_timeout_;
        bool                            connection_parameters_request_pending_;

        // default configuration parameters
        typedef                         advertising_interval< 100 >         default_advertising_interval;
        typedef                         sleep_clock_accuracy_ppm< 500 >     default_sleep_clock_accuracy;
        typedef                         random_static_address               default_device_address;

        typedef typename ::bluetoe::details::find_by_meta_type<
            details::device_address_meta_type,
            Options..., default_device_address >::type              local_device_address;

        typedef typename ::bluetoe::details::find_by_meta_type<
            details::sleep_clock_accuracy_meta_type,
            Options..., default_sleep_clock_accuracy >::type        device_sleep_clock_accuracy;

    };

    // implementation
    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    link_layer< Server, ScheduledRadio, Options... >::link_layer()
        : address_( local_device_address::address( *this ) )
        , current_channel_index_( first_advertising_channel )
        , defered_ll_control_pdu_{ nullptr, 0 }
        , server_( nullptr )
        , connection_details_( details::mtu_size< Options... >::mtu )
        , used_features_( supported_features )
        , state_( state::initial )
        , connection_parameters_request_pending_( false )
    {
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::run( Server& server )
    {
        // after the initial scheduling, the timeout and receive callback will setup the next scheduling
        if ( state_ == state::initial )
        {
            server_ = &server;
            start_advertising_impl();

            server.notification_callback( lcap_notification_callback, this );
        }

        radio_t::run();

        if ( state_ == state::connected )
        {
            transmit_notifications();
            transmit_signaling_channel_output();
            transmit_pending_control_pdus();
        }

        this->handle_connection_events();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::adv_received( const read_buffer& receive )
    {
        assert( state_ == state::advertising );

        device_address remote_address;
        const bool connection_request_received = this->handle_adv_receive( receive, remote_address );

        if ( connection_request_received
          && channels_.reset( &receive.buffer[ 30 ], receive.buffer[ 35 ] & 0x1f )
          && parse_timing_parameters_from_connect_request( receive ) )
        {
            state_                    = state::connecting;
            current_channel_index_    = 0;
            conn_event_counter_       = 0;
            cumulated_sleep_clock_accuracy_ = sleep_clock_accuracy( receive ) + device_sleep_clock_accuracy::accuracy_ppm;
            timeouts_til_connection_lost_   = num_windows_til_timeout - 1;
            used_features_            = supported_features;
            connection_parameters_request_pending_ = false;

            this->set_access_address_and_crc_init(
                read_32( &receive.buffer[ 14 ] ),
                read_24( &receive.buffer[ 18 ] ) );

            const delta_time window_start = transmit_window_offset_ - transmit_window_offset_.ppm( cumulated_sleep_clock_accuracy_ );
                  delta_time window_end   = transmit_window_offset_ + transmit_window_size_;

            window_end += window_end.ppm( cumulated_sleep_clock_accuracy_ );

            this->reset();
            this->schedule_connection_event(
                channels_.data_channel( current_channel_index_ ),
                window_start,
                window_end,
                connection_interval_ );

            this->connection_request( connection_addresses( address_, remote_address ) );
            this->handle_stop_advertising();

            connection_details_ = notification_queue_t( details::mtu_size< Options... >::mtu );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::adv_timeout()
    {
        assert( state_ == state::advertising );

        this->handle_adv_timeout();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::timeout()
    {
        assert( state_ == state::connecting || state_ == state::connected || state_ == state::connection_update || state_ == state::disconnecting );

        if ( timeouts_til_connection_lost_ )
        {
            current_channel_index_ = ( current_channel_index_ + 1 ) % first_advertising_channel;

            --timeouts_til_connection_lost_;
            ++conn_event_counter_;

            if ( handle_pending_ll_control() == ll_result::disconnect )
            {
                force_disconnect();
            }
            else
            {
                wait_for_connection_event();
            }
        }
        else
        {
            force_disconnect();
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::end_event()
    {
        assert( state_ == state::connecting || state_ == state::connected || state_ == state::connection_update || state_ == state::disconnecting );

        if ( state_ == state::connecting )
        {
            this->connection_established( details(), connection_details_, static_cast< radio_t& >( *this ) );
        }

        if ( state_ != state::disconnecting )
        {
            state_                        = state::connected;
            timeouts_til_connection_lost_ = max_timeouts_til_connection_lost_;
        }

        current_channel_index_        = ( current_channel_index_ + 1 ) % first_advertising_channel;
        ++conn_event_counter_;

        if( handle_received_data() == ll_result::disconnect || send_control_pdus() == ll_result::disconnect )
        {
            force_disconnect();
        }
        else
        {
            wait_for_connection_event();
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout )
    {
        if ( used_features_ & link_layer_feature::connection_parameters_request_procedure )
        {
            if ( connection_parameters_request_pending_ )
                return false;

            proposed_interval_min_  = interval_min;
            proposed_interval_max_  = interval_max;
            proposed_latency_       = latency;
            proposed_timeout_       = timeout;
            connection_parameters_request_pending_ = true;

            this->wake_up();

            return true;
        }

        const bool result = static_cast< signaling_channel_t& >( *this ).connection_parameter_update_request( interval_min, interval_max, latency, timeout );

        if ( result )
            this->wake_up();

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::disconnect()
    {
        state_            = state::disconnecting;
        termination_send_ = false;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::wait_for_connection_event()
    {
        delta_time window_start;
        delta_time window_end;

        if ( state_ == state::connecting )
        {
            const delta_time window_target = connection_interval_ * ( num_windows_til_timeout - timeouts_til_connection_lost_ - 1 );

            window_start = transmit_window_offset_ + window_target;
            window_end   = window_start + transmit_window_size_;

            window_start -= window_start.ppm( cumulated_sleep_clock_accuracy_ );
            window_end   += window_end.ppm( cumulated_sleep_clock_accuracy_ );
        }
        else if ( state_ == state::connection_update )
        {
            window_start = connection_interval_old_ + transmit_window_offset_;
            window_end   = window_start + transmit_window_size_;

            window_start -= window_start.ppm( cumulated_sleep_clock_accuracy_ );
            window_end   += window_end.ppm( cumulated_sleep_clock_accuracy_ );
        }
        else
        {
            const delta_time window_target = connection_interval_ * ( max_timeouts_til_connection_lost_ - timeouts_til_connection_lost_ + 1 );
            const delta_time window_size   = window_target.ppm( cumulated_sleep_clock_accuracy_ );

            window_start  = window_target - window_size;
            window_end    = window_target + window_size;
        }

        this->schedule_connection_event(
            channels_.data_channel( current_channel_index_ ),
            window_start,
            window_end,
            connection_interval_ );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_notifications()
    {
        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_transmit_buffer();

        if ( out_buffer.empty() )
            return;

        const auto notification = connection_details_.dequeue_indication_or_confirmation();

        if ( notification.first != notification_queue_t::empty )
        {
            std::size_t out_size = out_buffer.size - all_header_size;

            if ( notification.first == notification_queue_t::notification )
            {
                server_->notification_output(
                    &out_buffer.buffer[ all_header_size ],
                    out_size,
                    connection_details_,
                    notification.second
                );
            }
            else
            {
                server_->indication_output(
                    &out_buffer.buffer[ all_header_size ],
                    out_size,
                    connection_details_,
                    notification.second
                );

                // if no output is generate, confirm the indication, or we will wait for ever
                if ( out_size == 0 )
                    connection_details_.indication_confirmed();

            }

            if ( out_size )
            {
                out_buffer.buffer[ 0 ] = lld_data_pdu_code;
                out_buffer.buffer[ 1 ] = static_cast< std::uint8_t >( out_size + l2cap_header_size );
                out_buffer.buffer[ 2 ] = static_cast< std::uint8_t >( out_size );
                out_buffer.buffer[ 3 ] = 0;
                out_buffer.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_att_channel );
                out_buffer.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_att_channel >> 8 );

                this->commit_transmit_buffer( out_buffer );
            }
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_signaling_channel_output()
    {
        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_transmit_buffer();

        if ( out_buffer.empty() )
            return;

        std::size_t out_size = out_buffer.size - all_header_size;
        this->signaling_channel_output(
            &out_buffer.buffer[ all_header_size ],
            out_size );

        if ( out_size )
        {
            out_buffer.buffer[ 0 ] = lld_data_pdu_code;
            out_buffer.buffer[ 1 ] = static_cast< std::uint8_t >( out_size + l2cap_header_size );
            out_buffer.buffer[ 2 ] = static_cast< std::uint8_t >( out_size );
            out_buffer.buffer[ 3 ] = 0;
            out_buffer.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_signaling_channel );
            out_buffer.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_signaling_channel >> 8 );

            this->commit_transmit_buffer( out_buffer );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_pending_control_pdus()
    {
        if ( !connection_parameters_request_pending_ )
            return;

        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_transmit_buffer();

        if ( out_buffer.empty() )
        {
            this->wake_up();
            return;
        }

        connection_parameters_request_pending_ = false;

        out_buffer.fill( {
            ll_control_pdu_code, 22, LL_CONNECTION_PARAM_REQ,
            static_cast< std::uint8_t >( proposed_interval_min_ ),
            static_cast< std::uint8_t >( proposed_interval_min_ >> 8 ),
            static_cast< std::uint8_t >( proposed_interval_max_ ),
            static_cast< std::uint8_t >( proposed_interval_max_ >> 8 ),
            static_cast< std::uint8_t >( proposed_latency_ ),
            static_cast< std::uint8_t >( proposed_latency_ >> 8 ),
            static_cast< std::uint8_t >( proposed_timeout_ ),
            static_cast< std::uint8_t >( proposed_timeout_ >> 8 ),
            0x00,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff } );

        this->commit_transmit_buffer( out_buffer );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::lcap_notification_callback( const ::bluetoe::details::notification_data& item, void* usr_arg, typename Server::notification_type type )
    {
        auto& confirmation_queue = static_cast< link_layer< Server, ScheduledRadio, Options... >* >( usr_arg )->connection_details_;
        switch ( type )
        {
            case Server::notification:
                return confirmation_queue.queue_notification( item.client_characteristic_configuration_index() );
                break;
            case Server::indication:
                return confirmation_queue.queue_indication( item.client_characteristic_configuration_index() );
                break;
            case Server::confirmation:
                confirmation_queue.indication_confirmed();
                return true;
                break;
        }

        return true;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    unsigned link_layer< Server, ScheduledRadio, Options... >::sleep_clock_accuracy( const read_buffer& receive ) const
    {
        static constexpr std::uint16_t inaccuracy_ppm[ 8 ] = {
            500, 250, 150, 100, 75, 50, 30, 20
        };

        return inaccuracy_ppm[ ( receive.buffer[ 35 ] >> 5 & 0x7 )  ];
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::check_timing_paremeters( std::uint16_t slave_latency, delta_time timeout ) const
    {
        static constexpr delta_time maximum_transmit_window_offset( 10 * 1000 );
        static constexpr delta_time maximum_connection_timeout( 32 * 1000 * 1000 );
        static constexpr delta_time minimum_connection_timeout( 100 * 1000 );
        static constexpr auto       max_slave_latency = 499;

        return transmit_window_size_ <= maximum_transmit_window_offset
            && transmit_window_size_ <= connection_interval_
            && transmit_window_offset_ <= connection_interval_
            && timeout >= minimum_connection_timeout
            && timeout <= maximum_connection_timeout
            && timeout >= ( slave_latency + 1 ) * 2 * connection_interval_
            && slave_latency <= max_slave_latency;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::parse_timing_parameters_from_connect_request( const read_buffer& valid_connect_request )
    {
        static constexpr auto       us_per_digits = 1250;

        transmit_window_size_   = delta_time( valid_connect_request.buffer[ 21 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_connect_request.buffer[ 22 ] ) * us_per_digits + us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_connect_request.buffer[ 24 ] ) * us_per_digits );
        slave_latency_          = read_16( &valid_connect_request.buffer[ 26 ] );
        timeout_value_          = read_16( &valid_connect_request.buffer[ 28 ] );
        delta_time timeout      = delta_time( timeout_value_ * 10000 );

        max_timeouts_til_connection_lost_ = timeout / connection_interval_;

        return check_timing_paremeters( slave_latency_, timeout );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::parse_timing_parameters_from_connection_update_request( const write_buffer& valid_update_request )
    {
        static constexpr auto       us_per_digits = 1250;

        transmit_window_size_   = delta_time( valid_update_request.buffer[ 3 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_update_request.buffer[ 4 ] ) * us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_update_request.buffer[ 6 ] ) * us_per_digits );
        slave_latency_          = read_16( &valid_update_request.buffer[ 8 ] );
        timeout_value_          = read_16( &valid_update_request.buffer[ 10 ] );
        delta_time timeout      = delta_time( timeout_value_ * 10000 );

        max_timeouts_til_connection_lost_ = timeout / connection_interval_;

        return check_timing_paremeters( slave_latency_, timeout );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::force_disconnect()
    {
        this->connection_closed( connection_details_, static_cast< radio_t& >( *this ) );

        start_advertising_impl();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::start_advertising_impl()
    {
        state_ = state::advertising;

        defered_ll_control_pdu_ = write_buffer{ nullptr, 0 };

        this->handle_start_advertising();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_received_data()
    {
        ll_result result = handle_pending_ll_control();

        if ( result != ll_result::go_ahead )
            return result;

        if ( defered_ll_control_pdu_.empty() )
        {
            for ( auto pdu = this->next_received(); pdu.size != 0; )
            {
                auto output = this->allocate_transmit_buffer();

                if ( output.size )
                {
                    const auto llid = pdu.buffer[ 0 ] & 0x03;
                    if ( llid == ll_control_pdu_code )
                    {
                        result = handle_ll_control_data( pdu, output );
                    }
                    else if ( llid == lld_data_pdu_code && state_ != state::disconnecting )
                    {
                        result = handle_l2cap( pdu, output );
                    }

                    this->free_received();
                    pdu = this->next_received();
                }
                else
                {
                    pdu.size = 0;
                }
            }
        }

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::send_control_pdus()
    {
        static constexpr std::uint8_t connection_terminated_by_local_host = 0x16;

        if ( state_ == state::disconnecting && !termination_send_ )
        {
            auto output = this->allocate_transmit_buffer();

            if ( output.size )
            {
                output.fill( {
                    ll_control_pdu_code, 2,
                    LL_TERMINATE_IND, connection_terminated_by_local_host
                } );

                this->commit_transmit_buffer( output );
                termination_send_ = true;
            }
        }

        return ll_result::go_ahead;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_ll_control_data( const write_buffer& pdu, read_buffer write )
    {
        ll_result result = ll_result::go_ahead;
        bool      commit = true;

        assert( write.size >= radio_t::min_buffer_size );

        if ( ( pdu.buffer[ 0 ] & 0x3 ) == ll_control_pdu_code )
        {

            const std::uint8_t size   = pdu.buffer[ 1 ];
            const std::uint8_t opcode = size > 0 ? pdu.buffer[ 2 ] : 0xff;

            if ( opcode == LL_CONNECTION_UPDATE_REQ && size == 12 )
            {
                defered_conn_event_counter_ = read_16( &pdu.buffer[ 12 ] );
                commit = false;

                if ( static_cast< std::uint16_t >( defered_conn_event_counter_ - conn_event_counter_ ) & 0x8000
                    || defered_conn_event_counter_ == conn_event_counter_ )
                {
                    result = ll_result::disconnect;
                }
                else
                {
                    defered_ll_control_pdu_ = pdu;
                }
            }
            else if ( opcode == LL_TERMINATE_IND && size == 2 )
            {
                commit = false;
                result = ll_result::disconnect;
            }
            else if ( opcode == LL_VERSION_IND && size == 6 )
            {
                if ( pdu.buffer[ 3 ] <= LL_VERSION_40 )
                    used_features_ = used_features_ & ~link_layer_feature::connection_parameters_request_procedure;

                write.fill( {
                    ll_control_pdu_code, 6, LL_VERSION_IND,
                    LL_VERSION_NR, 0x69, 0x02, 0x00, 0x00
                } );
            }
            else if ( opcode == LL_CHANNEL_MAP_REQ && size == 8 )
            {
                defered_conn_event_counter_ = read_16( &pdu.buffer[ 8 ] );
                commit = false;

                if ( static_cast< std::uint16_t >( defered_conn_event_counter_ - conn_event_counter_ ) & 0x8000 )
                {
                    result = ll_result::disconnect;
                }
                else
                {
                    defered_ll_control_pdu_ = pdu;
                }
            }
            else if ( opcode == LL_PING_REQ && size == 1 )
            {
                write.fill( { ll_control_pdu_code, 1, LL_PING_RSP } );
            }
            else if ( opcode == LL_FEATURE_REQ && size == 9 )
            {
                used_features_ = used_features_ & pdu.buffer[ 3 ];

                write.fill( {
                    ll_control_pdu_code, 9,
                    LL_FEATURE_RSP,
                    used_features_,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                } );
            }
            else if ( opcode != LL_UNKNOWN_RSP )
            {
                write.fill( { ll_control_pdu_code, 2, LL_UNKNOWN_RSP, opcode } );
            }
            else
            {
                commit = false;
            }

            if ( commit )
                this->commit_transmit_buffer( write );
        }

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_l2cap( const write_buffer& input, const read_buffer& output )
    {
        const std::uint8_t  pdu_size      = input.buffer[ 1 ];
        const std::uint16_t l2cap_size    = read_16( &input.buffer[ 2 ] );
        const std::uint16_t l2cap_channel = read_16( &input.buffer[ 4 ] );

        if ( pdu_size - l2cap_header_size != l2cap_size )
            return ll_result::disconnect;

        if ( l2cap_channel == l2cap_att_channel )
        {
            std::size_t att_size = output.size - all_header_size;

            server_->l2cap_input( &input.buffer[ all_header_size ], l2cap_size, &output.buffer[ all_header_size ], att_size, connection_details_ );

            if ( att_size )
            {
                output.buffer[ 0 ] = lld_data_pdu_code;
                output.buffer[ 1 ] = static_cast< std::uint8_t >( att_size + l2cap_header_size );
                output.buffer[ 2 ] = static_cast< std::uint8_t >( att_size );
                output.buffer[ 3 ] = 0;
                output.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_att_channel );
                output.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_att_channel >> 8 );

                this->commit_transmit_buffer( output );
            }
        }
        else if ( l2cap_channel == l2cap_sm_channel )
        {
            std::size_t sm_size = output.size - all_header_size;

            static_cast< security_manager_t& >( *this ).l2cap_input( &input.buffer[ all_header_size ], l2cap_size, &output.buffer[ all_header_size ], sm_size );

            if ( sm_size )
            {
                output.buffer[ 0 ] = lld_data_pdu_code;
                output.buffer[ 1 ] = static_cast< std::uint8_t >( sm_size + l2cap_header_size );
                output.buffer[ 2 ] = static_cast< std::uint8_t >( sm_size );
                output.buffer[ 3 ] = 0;
                output.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_sm_channel );
                output.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_sm_channel >> 8 );

                this->commit_transmit_buffer( output );
            }
        }
        else if ( l2cap_channel == l2cap_signaling_channel )
        {
            std::size_t signaling_size = output.size - all_header_size;

            this->signaling_channel_input(
                &input.buffer[ all_header_size ], l2cap_size, &output.buffer[ all_header_size ], signaling_size );

            if ( signaling_size )
            {
                output.buffer[ 0 ] = lld_data_pdu_code;
                output.buffer[ 1 ] = static_cast< std::uint8_t >( signaling_size + l2cap_header_size );
                output.buffer[ 2 ] = static_cast< std::uint8_t >( signaling_size );
                output.buffer[ 3 ] = 0;
                output.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_signaling_channel );
                output.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_signaling_channel >> 8 );

                this->commit_transmit_buffer( output );
            }
        }

        return ll_result::go_ahead;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_pending_ll_control()
    {
        ll_result result = ll_result::go_ahead;

        if ( !defered_ll_control_pdu_.empty() && defered_conn_event_counter_ == conn_event_counter_ )
        {
            const std::uint8_t opcode = defered_ll_control_pdu_.buffer[ 2 ];

            if ( opcode == LL_CHANNEL_MAP_REQ )
            {
                channels_.reset( &defered_ll_control_pdu_.buffer[ 3 ] );
            }
            else if ( opcode == LL_CONNECTION_UPDATE_REQ )
            {
                connection_interval_old_ = connection_interval_;
                if ( parse_timing_parameters_from_connection_update_request( defered_ll_control_pdu_ ) )
                {
                    timeouts_til_connection_lost_ = 0;
                    state_ = state::connection_update;

                    this->connection_changed( details(), connection_details_, static_cast< radio_t& >( *this ) );
                }
                else
                {
                    result = ll_result::disconnect;
                }
            }
            else
            {
                assert( !"invalid opcode" );
            }

            defered_ll_control_pdu_ = write_buffer{ nullptr, 0 };
        }

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    connection_details link_layer< Server, ScheduledRadio, Options... >::details() const
    {
        return connection_details(
            channels_,
            connection_interval_.usec() / 1250,
            slave_latency_,
            timeout_value_,
            cumulated_sleep_clock_accuracy_ );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint16_t link_layer< Server, ScheduledRadio, Options... >::read_16( const std::uint8_t* p )
    {
        return *p | *( p + 1 ) << 8;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint32_t link_layer< Server, ScheduledRadio, Options... >::read_24( const std::uint8_t* p )
    {
        return static_cast< std::uint32_t >( read_16( p ) ) | static_cast< std::uint32_t >( *( p + 2 ) ) << 16;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint32_t link_layer< Server, ScheduledRadio, Options... >::read_32( const std::uint8_t* p )
    {
        return static_cast< std::uint32_t >( read_16( p ) ) | static_cast< std::uint32_t >( read_16( p + 2 ) ) << 16;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::size_t link_layer< Server, ScheduledRadio, Options... >::fill_l2cap_advertising_data( std::uint8_t* buffer, std::size_t buffer_size ) const
    {
        return server_->advertising_data( buffer, buffer_size );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    const device_address& link_layer< Server, ScheduledRadio, Options... >::local_address() const
    {
        return address_;
    }

}
}

#endif
