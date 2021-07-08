#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/ll_options.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/channel_map.hpp>
#include <bluetoe/notification_queue.hpp>
#include <bluetoe/connection_callbacks.hpp>
#include <bluetoe/connection_event_callback.hpp>
#include <bluetoe/l2cap_signaling_channel.hpp>
#include <bluetoe/white_list.hpp>
#include <bluetoe/advertising.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/meta_types.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/security_manager.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/encryption.hpp>

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

        template < typename Server, typename ... Options >
        struct security_manager {
            using default_sm = typename bluetoe::details::select_type<
                bluetoe::details::requires_encryption_support_t< Server >::value,
                bluetoe::security_manager,
                bluetoe::no_security_manager
            >::type;

            using type = typename bluetoe::details::find_by_meta_type<
                bluetoe::details::security_manager_meta_type,
                Options...,
                default_sm >::type;
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
                max_mtu_size< bluetoe::details::default_att_mtu_size > >::type type;

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

        /*
         * The Part of link layer, that handles security related stuff is
         * factored out, to not have unnessary code, in case that no
         * security relevant code is required
         */
        struct link_layer_security_impl
        {
            template < class LinkLayer >
            class impl
            {
            public:
                impl()
                    : has_key_( false )
                    , encryption_in_progress_( false )
                {}

                LinkLayer& that()
                {
                    return static_cast< LinkLayer& >( *this );
                }

                bool handle_encryption_pdus( std::uint8_t opcode, std::uint8_t size, const write_buffer& pdu, read_buffer write )
                {
                    using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                    bool encryption_changed = false;
                    bool result = true;

                    if ( opcode == LinkLayer::LL_ENC_REQ && size == 23 )
                    {
                        encryption_in_progress_ = true;
                        fill< layout_t >( write, { LinkLayer::ll_control_pdu_code, 1 + 8 + 4, LinkLayer::LL_ENC_RSP } );

                        const std::uint8_t* const pdu_body = layout_t::body( pdu ).first;

                        const std::uint64_t rand = LinkLayer::read_64( pdu_body +1 );
                        const std::uint16_t ediv = LinkLayer::read_16( pdu_body +9 );
                        const std::uint64_t skdm = LinkLayer::read_64( pdu_body +11 );
                        const std::uint32_t ivm  = LinkLayer::read_32( pdu_body +19 );
                              std::uint64_t skds = 0;
                              std::uint32_t ivs  = 0;

                        bluetoe::details::uint128_t key;
                        std::tie( has_key_, key ) = that().connection_details_.find_key( ediv, rand );

                        // setup encryption
                        std::tie( skds, ivs ) = that().setup_encryption( key, skdm, ivm );

                        std::uint8_t* write_body = layout_t::body( write ).first;
                        bluetoe::details::write_64bit( &write_body[ 1 ], skds );
                        bluetoe::details::write_32bit( &write_body[ 9 ], ivs );
                    }
                    else if ( opcode == LinkLayer::LL_START_ENC_RSP && size == 1 )
                    {
                        fill< layout_t >( write, { LinkLayer::ll_control_pdu_code, 1, LinkLayer::LL_START_ENC_RSP } );
                        that().start_transmit_encrypted();
                        encryption_changed = that().connection_details_.is_encrypted( true );
                    }
                    else if ( opcode == LinkLayer::LL_PAUSE_ENC_REQ && size == 1 )
                    {
                        fill< layout_t >( write, { LinkLayer::ll_control_pdu_code, 1, LinkLayer::LL_PAUSE_ENC_RSP } );
                        that().stop_receive_encrypted();
                        encryption_changed = that().connection_details_.is_encrypted( false );
                    }
                    else if ( opcode == LinkLayer::LL_PAUSE_ENC_RSP && size == 1 )
                    {
                        that().stop_transmit_encrypted();
                        encryption_changed = that().connection_details_.is_encrypted( false );

                        result = false;
                    }
                    else
                    {
                        result = false;
                    }

                    if ( encryption_changed )
                        that().connection_changed( that().details(), that().connection_details_, static_cast< typename LinkLayer::radio_t& >( that() ) );

                    return result;
                }

                void transmit_pending_security_pdus()
                {
                    using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                    if ( !encryption_in_progress_ )
                        return;

                    auto out_buffer = that().allocate_transmit_buffer();
                    if ( out_buffer.empty() )
                        return;

                    if ( has_key_ )
                    {
                        fill< layout_t >( out_buffer, {
                            LinkLayer::ll_control_pdu_code, 1, LinkLayer::LL_START_ENC_REQ } );

                        that().start_receive_encrypted();
                        that().commit_transmit_buffer( out_buffer );
                    }
                    else
                    {
                        that().reject( LinkLayer::LL_ENC_REQ, LinkLayer::err_pin_or_key_missing, out_buffer );
                        that().commit_transmit_buffer( out_buffer );
                    }

                    encryption_in_progress_ = false;
                }

                void reset_encryption()
                {
                    that().connection_details_.is_encrypted( false );
                    that().stop_receive_encrypted();
                    that().stop_transmit_encrypted();
                }

            private:
                bool has_key_;
                bool encryption_in_progress_;
            };
        };

        struct link_layer_no_security_impl
        {
            template < class LinkLayer >
            struct impl
            {
                bool handle_encryption_pdus( std::uint8_t, std::uint8_t, write_buffer, read_buffer )
                {
                    return false;
                }

                void transmit_pending_security_pdus()
                {
                }

                void reset_encryption()
                {
                }
            };
        };

        template < class Server, class LinkLayer >
        using select_link_layer_security_impl =
            typename bluetoe::details::select_type<
                bluetoe::details::requires_encryption_support_t< Server >::value,
                link_layer_security_impl,
                link_layer_no_security_impl
            >::type::template impl< LinkLayer >;
    }

    /**
     * @brief link layer implementation
     *
     * Implements a binding to a server by implementing a link layer on top of a ScheduleRadio device.
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
        public details::security_manager< Server, Options... >::type,
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
        private details::signaling_channel< Options... >::type,
        private details::select_link_layer_security_impl< Server, link_layer< Server, ScheduledRadio, Options... > >
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
         * @brief fills the given buffer with l2cap scan response payload
         */
        std::size_t fill_l2cap_scan_response_data( std::uint8_t* buffer, std::size_t buffer_size ) const;

        /**
         * @brief returns the own local device address
         */
        const device_address& local_address() const;

        using radio_t = ScheduledRadio<
            details::buffer_sizes< Options... >::tx_size,
            details::buffer_sizes< Options... >::rx_size,
            link_layer< Server, ScheduledRadio, Options... > >;

        using layout_t = typename pdu_layout_by_radio< radio_t >::pdu_layout;

    private:

        friend details::select_link_layer_security_impl< Server, link_layer< Server, ScheduledRadio, Options... > >;

        static_assert(
            std::is_same<
                typename ::bluetoe::details::find_by_not_meta_type<
                    details::valid_link_layer_option_meta_type,
                    Options...
                >::type, ::bluetoe::details::no_such_type >::value,
            "Option passed to the link layer, that is not a valid link_layer option." );

        // make sure, that the hardware supports encryption
        static constexpr bool encryption_required = bluetoe::details::requires_encryption_support_t< Server >::value;
        static_assert( !encryption_required || ( encryption_required && radio_t::hardware_supports_encryption ),
            "The GATT server requires encryption while the selecte hardware binding doesn't provide support for encryption!" );

        typedef typename details::security_manager< Server, Options... >::type security_manager_t;

        typedef notification_queue<
            typename Server::notification_priority::template numbers< typename Server::services >::type,
            typename Server::connection_data > notification_queue_t;

        typedef typename security_manager_t::template connection_data< notification_queue_t > connection_details_t;

        typedef typename details::signaling_channel< Options... >::type signaling_channel_t;

        typedef details::select_advertiser_implementation<
            link_layer< Server, ScheduledRadio, Options... >, Options... > advertising_t;

        unsigned sleep_clock_accuracy( const std::uint8_t* received_body ) const;
        bool check_timing_paremeters() const;
        bool parse_timing_parameters_from_connect_request( const std::uint8_t* valid_connect_request_body );
        bool parse_timing_parameters_from_connection_update_request( const std::uint8_t* valid_connect_request );
        void force_disconnect();
        void start_advertising_impl();
        void wait_for_connection_event();
        void transmit_notifications();
        void transmit_signaling_channel_output();
        void transmit_pending_control_pdus();
        void reject( std::uint8_t opcode, std::uint8_t error_code, read_buffer& output );

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
        static std::uint64_t read_64( const std::uint8_t* );

        static constexpr unsigned       first_advertising_channel   = 37;
        static constexpr unsigned       num_windows_til_timeout     = 5;
        static constexpr auto           us_per_digits               = 1250;

        static constexpr std::uint8_t   ll_control_pdu_code         = 3;
        static constexpr std::uint8_t   lld_data_pdu_code           = 2;

        static constexpr std::uint8_t   LL_CONNECTION_UPDATE_REQ    = 0x00;
        static constexpr std::uint8_t   LL_CHANNEL_MAP_REQ          = 0x01;
        static constexpr std::uint8_t   LL_TERMINATE_IND            = 0x02;
        static constexpr std::uint8_t   LL_ENC_REQ                  = 0x03;
        static constexpr std::uint8_t   LL_ENC_RSP                  = 0x04;
        static constexpr std::uint8_t   LL_START_ENC_REQ            = 0x05;
        static constexpr std::uint8_t   LL_START_ENC_RSP            = 0x06;
        static constexpr std::uint8_t   LL_UNKNOWN_RSP              = 0x07;
        static constexpr std::uint8_t   LL_FEATURE_REQ              = 0x08;
        static constexpr std::uint8_t   LL_FEATURE_RSP              = 0x09;
        static constexpr std::uint8_t   LL_PAUSE_ENC_REQ            = 0x0A;
        static constexpr std::uint8_t   LL_PAUSE_ENC_RSP            = 0x0B;
        static constexpr std::uint8_t   LL_VERSION_IND              = 0x0C;
        static constexpr std::uint8_t   LL_REJECT_IND               = 0x0D;
        static constexpr std::uint8_t   LL_CONNECTION_PARAM_REQ     = 0x0F;
        static constexpr std::uint8_t   LL_CONNECTION_PARAM_RSP     = 0x10;
        static constexpr std::uint8_t   LL_REJECT_IND_EXT           = 0x11;
        static constexpr std::uint8_t   LL_PING_REQ                 = 0x12;
        static constexpr std::uint8_t   LL_PING_RSP                 = 0x13;

        static constexpr std::uint8_t   LL_VERSION_NR               = 0x08;
        static constexpr std::uint8_t   LL_VERSION_40               = 0x06;

        static constexpr std::uint16_t  l2cap_att_channel           = 4;
        static constexpr std::uint16_t  l2cap_signaling_channel     = 5;
        static constexpr std::uint16_t  l2cap_sm_channel            = 6;

        static constexpr std::size_t    l2cap_header_size           = 4;
        static constexpr std::size_t    all_header_size             = 6;

        static constexpr std::uint8_t   err_pin_or_key_missing      = 0x06;

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
            link_layer_feature::extended_reject_indication |
            link_layer_feature::le_ping |
            ( bluetoe::details::requires_encryption_support_t< Server >::value
                ? link_layer_feature::le_encryption
                : 0 );

        // TODO: calculate the actual needed buffer size for advertising, not the maximum
        static_assert( radio_t::size >= advertising_t::maximum_required_advertising_buffer(), "buffer to small" );

        const device_address            address_;
        unsigned                        current_channel_index_;
        channel_map                     channels_;
        unsigned                        cumulated_sleep_clock_accuracy_;
        delta_time                      transmit_window_offset_;
        delta_time                      transmit_window_size_;
        delta_time                      connection_interval_;
        std::uint16_t                   slave_latency_;
        std::uint16_t                   timeout_value_;
        delta_time                      time_till_next_event_;
        delta_time                      connection_timeout_;
        std::uint16_t                   conn_event_counter_;
        std::uint16_t                   defered_conn_event_counter_;
        write_buffer                    defered_ll_control_pdu_;
        Server*                         server_;
        connection_details_t            connection_details_;
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
        bool                            connection_parameters_request_running_;

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

        typedef typename ::bluetoe::details::find_by_meta_type<
            details::connection_event_callback_meta_type,
            Options..., details::default_connection_event_callback
        >::type                                                     connection_event_callback;
    };

    // implementation
    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    link_layer< Server, ScheduledRadio, Options... >::link_layer()
        : address_( local_device_address::address( *this ) )
        , current_channel_index_( first_advertising_channel )
        , defered_ll_control_pdu_{ nullptr, 0 }
        , server_( nullptr )
        , connection_details_( std::size_t{ details::mtu_size< Options... >::mtu } )
        , used_features_( supported_features )
        , state_( state::initial )
        , connection_parameters_request_pending_( false )
        , connection_parameters_request_running_( false )
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

        if ( connection_request_received )
        {
            const std::uint8_t* const body = layout_t::body( receive ).first;

            if ( channels_.reset( &body[ 28 ], body[ 33 ] & 0x1f )
              && parse_timing_parameters_from_connect_request( body ) )
            {
                state_                    = state::connecting;
                current_channel_index_    = 0;
                conn_event_counter_       = 0;
                cumulated_sleep_clock_accuracy_ = sleep_clock_accuracy( body ) + device_sleep_clock_accuracy::accuracy_ppm;
                used_features_            = supported_features;
                connection_parameters_request_pending_ = false;
                connection_parameters_request_running_ = false;
                time_till_next_event_     = delta_time();

                this->set_access_address_and_crc_init( read_32( &body[ 12 ] ), read_24( &body[ 16 ] ) );

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

                connection_details_ = connection_details_t( std::size_t{ details::mtu_size< Options... >::mtu } );
                connection_details_.remote_connection_created( remote_address );
            }
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

        if ( time_till_next_event_ <= connection_timeout_
            && !( state_ == state::connecting && time_till_next_event_ >= ( num_windows_til_timeout - 1 ) * connection_interval_ ) )
        {
            time_till_next_event_ += connection_interval_;

            current_channel_index_ = ( current_channel_index_ + 1 ) % first_advertising_channel;
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

        time_till_next_event_ = connection_interval_;

        if ( state_ == state::connecting )
        {
            this->connection_established( details(), connection_details_, static_cast< radio_t& >( *this ) );
        }

        if ( state_ != state::disconnecting )
        {
            state_                        = state::connected;
        }

        current_channel_index_        = ( current_channel_index_ + 1 ) % first_advertising_channel;
        ++conn_event_counter_;

        if ( handle_received_data() == ll_result::disconnect || send_control_pdus() == ll_result::disconnect )
        {
            force_disconnect();
        }
        else
        {
            this->transmit_pending_security_pdus();
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

        this->reset_encryption();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::wait_for_connection_event()
    {
        delta_time window_start;
        delta_time window_end;

        if ( state_ == state::connecting || state_ == state::connection_update )
        {
            window_start = transmit_window_offset_ + time_till_next_event_;
            window_end   = window_start + transmit_window_size_;

            window_start -= window_start.ppm( cumulated_sleep_clock_accuracy_ );
            window_end   += window_end.ppm( cumulated_sleep_clock_accuracy_ );
        }
        else
        {
            const delta_time window_size   = time_till_next_event_.ppm( cumulated_sleep_clock_accuracy_ );

            window_start  = time_till_next_event_ - window_size;
            window_end    = time_till_next_event_ + window_size;
        }

        const delta_time time_till_next_event = this->schedule_connection_event(
                channels_.data_channel( current_channel_index_ ),
                window_start,
                window_end,
                connection_interval_ );

        // Do not call the connection callback, if the last connection timed out, as this could be an indication for
        // the callback constantly consuming too much CPU time
        if ( time_till_next_event_ > connection_interval_ )
        {
            connection_event_callback::call_connection_event_callback( time_till_next_event );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_notifications()
    {
        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_transmit_buffer();

        if ( out_buffer.empty() )
            return;

        const auto notification = connection_details_.dequeue_indication_or_confirmation();

        if ( notification.first != connection_details_t::entry_type::empty )
        {
            std::size_t   out_size = out_buffer.size - all_header_size - layout_t::data_channel_pdu_memory_size( 0 );
            std::uint8_t* out_body = layout_t::body( out_buffer ).first;

            if ( notification.first == connection_details_t::entry_type::notification )
            {
                server_->notification_output(
                    &out_body[ l2cap_header_size ],
                    out_size,
                    connection_details_,
                    notification.second
                );
            }
            else
            {
                server_->indication_output(
                    &out_body[ l2cap_header_size ],
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
                fill< layout_t >( out_buffer, {
                    lld_data_pdu_code,
                    static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                    static_cast< std::uint8_t >( out_size ),
                    0,
                    static_cast< std::uint8_t >( l2cap_att_channel ),
                    static_cast< std::uint8_t >( l2cap_att_channel >> 8 ) } );

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

        std::size_t   out_size = out_buffer.size - all_header_size - layout_t::data_channel_pdu_memory_size( 0 );
        std::uint8_t* out_body = layout_t::body( out_buffer ).first;

        this->signaling_channel_output( &out_body[ l2cap_header_size ], out_size );

        if ( out_size )
        {
            fill< layout_t >( out_buffer, {
                lld_data_pdu_code,
                static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                static_cast< std::uint8_t >( out_size ),
                0,
                static_cast< std::uint8_t >( l2cap_signaling_channel ),
                static_cast< std::uint8_t >( l2cap_signaling_channel >> 8 ) } );

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
        connection_parameters_request_running_ = true;

        fill< layout_t >( out_buffer, {
            ll_control_pdu_code, 24, LL_CONNECTION_PARAM_REQ,
            static_cast< std::uint8_t >( proposed_interval_min_ ),
            static_cast< std::uint8_t >( proposed_interval_min_ >> 8 ),
            static_cast< std::uint8_t >( proposed_interval_max_ ),
            static_cast< std::uint8_t >( proposed_interval_max_ >> 8 ),
            static_cast< std::uint8_t >( proposed_latency_ ),
            static_cast< std::uint8_t >( proposed_latency_ >> 8 ),
            static_cast< std::uint8_t >( proposed_timeout_ ),
            static_cast< std::uint8_t >( proposed_timeout_ >> 8 ),
            0x00,                                   // PreferredPeriodicity (none)
            0x00, 0x00,                             // ReferenceConnEventCount
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff } );

        this->commit_transmit_buffer( out_buffer );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::reject( std::uint8_t opcode, std::uint8_t error_code, read_buffer& output )
    {
        if ( used_features_ & link_layer_feature::extended_reject_indication )
        {
            fill< layout_t >( output, {
                ll_control_pdu_code, 3, LL_REJECT_IND_EXT, opcode, error_code } );
        }
        else
        {
            fill< layout_t >( output, {
                ll_control_pdu_code, 2, LL_REJECT_IND, error_code } );
        }
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
    unsigned link_layer< Server, ScheduledRadio, Options... >::sleep_clock_accuracy( const std::uint8_t* received_body ) const
    {
        static constexpr std::uint16_t inaccuracy_ppm[ 8 ] = {
            500, 250, 150, 100, 75, 50, 30, 20
        };

        return inaccuracy_ppm[ ( received_body[ 33 ] >> 5 & 0x7 )  ];
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::check_timing_paremeters() const
    {
        static constexpr delta_time maximum_transmit_window_offset( 10 * 1000 );
        static constexpr delta_time maximum_connection_timeout( 32 * 1000 * 1000 );
        static constexpr delta_time minimum_connection_timeout( 100 * 1000 );
        static constexpr auto       max_slave_latency = 499;

        return transmit_window_size_ <= maximum_transmit_window_offset
            && transmit_window_size_ <= connection_interval_
            && connection_timeout_ >= minimum_connection_timeout
            && connection_timeout_ <= maximum_connection_timeout
            && connection_timeout_ >= ( slave_latency_ + 1 ) * 2 * connection_interval_
            && slave_latency_ <= max_slave_latency;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::parse_timing_parameters_from_connect_request( const std::uint8_t* valid_connect_request_body )
    {
        const delta_time transmit_window_offset = delta_time( read_16( &valid_connect_request_body[ 20 ] ) * us_per_digits );

        transmit_window_size_   = delta_time( valid_connect_request_body[ 19 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_connect_request_body[ 20 ] ) * us_per_digits + us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_connect_request_body[ 22 ] ) * us_per_digits );
        slave_latency_          = read_16( &valid_connect_request_body[ 24 ] );
        timeout_value_          = read_16( &valid_connect_request_body[ 26 ] );
        connection_timeout_     = delta_time( timeout_value_ * 10000 );

        return transmit_window_offset <= connection_interval_ && check_timing_paremeters();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::parse_timing_parameters_from_connection_update_request( const std::uint8_t* valid_update_request )
    {
        transmit_window_size_   = delta_time( valid_update_request[ 1 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_update_request[ 2 ] ) * us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_update_request[ 4 ] ) * us_per_digits );
        slave_latency_          = read_16( &valid_update_request[ 6 ] );
        timeout_value_          = read_16( &valid_update_request[ 8 ] );
        connection_timeout_     = delta_time( timeout_value_ * 10000 );

        return transmit_window_offset_ <= connection_interval_ && check_timing_paremeters();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::force_disconnect()
    {
        this->reset_encryption();
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

        if ( result != ll_result::go_ahead || !defered_ll_control_pdu_.empty() )
            return result;

        for ( auto pdu = this->next_received(); pdu.size != 0; )
        {
            auto output = this->allocate_transmit_buffer();

            if ( output.size )
            {
                const auto llid = layout_t::header( pdu ) & 0x03;

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
                fill< layout_t >( output, {
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

        const std::uint8_t* const body       = layout_t::body( pdu ).first;
        const std::uint16_t       header     = layout_t::header( pdu );
              std::uint8_t* const write_body = layout_t::body( write ).first;

        if ( ( header & 0x3 ) == ll_control_pdu_code )
        {

            const std::uint8_t size   = header >> 8;
            const std::uint8_t opcode = size > 0 ? *body : 0xff;

            if ( opcode == LL_CONNECTION_UPDATE_REQ && size == 12 )
            {
                defered_conn_event_counter_ = read_16( &body[ 10 ] );
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
                if ( body[ 1 ] <= LL_VERSION_40 )
                    used_features_ = used_features_ & ~link_layer_feature::connection_parameters_request_procedure;

                fill< layout_t >( write, {
                    ll_control_pdu_code, 6, LL_VERSION_IND,
                    LL_VERSION_NR, 0x69, 0x02, 0x00, 0x00
                } );
            }
            else if ( opcode == LL_CHANNEL_MAP_REQ && size == 8 )
            {
                defered_conn_event_counter_ = read_16( &body[ 6 ] );
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
                fill< layout_t >( write, { ll_control_pdu_code, 1, LL_PING_RSP } );
            }
            else if ( opcode == LL_FEATURE_REQ && size == 9 )
            {
                used_features_ = used_features_ & body[ 1 ];

                fill< layout_t >( write, {
                    ll_control_pdu_code, 9,
                    LL_FEATURE_RSP,
                    used_features_,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                } );
            }
            else if ( ( opcode == LL_UNKNOWN_RSP && size == 2 && body[ 1 ] == LL_CONNECTION_PARAM_REQ )
                || opcode == LL_REJECT_IND
                || ( opcode == LL_REJECT_IND_EXT && body[ 1 ] == LL_CONNECTION_PARAM_REQ ) )
            {
                if ( connection_parameters_request_running_ )
                {
                    connection_parameters_request_running_ = false;

                    if ( signaling_channel_t::connection_parameter_update_request(
                        proposed_interval_min_,
                        proposed_interval_max_,
                        proposed_latency_,
                        proposed_timeout_ ) )
                    {
                        this->wake_up();
                    }
                }

                if ( opcode == LL_UNKNOWN_RSP )
                    used_features_ = used_features_ & ~link_layer_feature::connection_parameters_request_procedure;

                commit = false;
            }
            else if ( opcode == LL_CONNECTION_PARAM_REQ && size == 24 )
            {
                fill< layout_t >( write, { ll_control_pdu_code, size, LL_CONNECTION_PARAM_RSP } );

                std::copy( &body[ 1 ], &body[ 1 + size - 1 ], &write_body[ 1 ] );
            }
            else if ( this->handle_encryption_pdus( opcode, size, pdu, write ) )
            {
                // all encryption PDU handled in handle_encryption_pdus()
            }
            else if ( opcode != LL_UNKNOWN_RSP )
            {
                fill< layout_t >( write, { ll_control_pdu_code, 2, LL_UNKNOWN_RSP, opcode } );
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
        const std::uint16_t input_header    = layout_t::header( input );
        const std::uint8_t* const input_body= layout_t::body( input ).first;
        const std::uint8_t  pdu_size        = input_header >> 8;

        const std::uint16_t l2cap_size      = read_16( &input_body[ 0 ] );
        const std::uint16_t l2cap_channel   = read_16( &input_body[ 2 ] );

        if ( pdu_size - l2cap_header_size != l2cap_size )
            return ll_result::disconnect;

        std::size_t   out_size   = output.size - l2cap_header_size - layout_t::data_channel_pdu_memory_size( 0 );
        std::uint8_t* out_body   = layout_t::body( output ).first;

        if ( l2cap_channel == l2cap_att_channel )
        {
            server_->l2cap_input( &input_body[ l2cap_header_size ], l2cap_size, &out_body[ l2cap_header_size ], out_size, connection_details_ );

            // in case the ATT input changed the MTU size:
            this->max_rx_size( connection_details_.negotiated_mtu() + all_header_size );
            this->max_tx_size( connection_details_.negotiated_mtu() + all_header_size );
        }
        else if ( l2cap_channel == l2cap_sm_channel )
        {
            static_cast< security_manager_t& >( *this ).l2cap_input( &input_body[ l2cap_header_size ], l2cap_size, &out_body[ l2cap_header_size ], out_size, connection_details_, *this );

            // in case the pairing status changed
            connection_details_.pairing_status( connection_details_.local_device_pairing_status() );
        }
        else if ( l2cap_channel == l2cap_signaling_channel )
        {
            this->signaling_channel_input(
                &input_body[ l2cap_header_size ], l2cap_size, &out_body[ l2cap_header_size ], out_size );
        }
        else
        {
            out_size = 0;
        }

        if ( out_size )
        {
            fill< layout_t >( output, {
                lld_data_pdu_code,
                static_cast< std::uint8_t >( out_size + l2cap_header_size ),
                static_cast< std::uint8_t >( out_size ),
                0,
                static_cast< std::uint8_t >( l2cap_channel ),
                static_cast< std::uint8_t >( l2cap_channel >> 8 ) } );

            this->commit_transmit_buffer( output );
        }

        return ll_result::go_ahead;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_pending_ll_control()
    {
        ll_result result = ll_result::go_ahead;

        if ( !defered_ll_control_pdu_.empty() && defered_conn_event_counter_ == conn_event_counter_ )
        {
            const std::uint8_t* body   = layout_t::body( defered_ll_control_pdu_ ).first;
            const std::uint8_t  opcode = body[ 0 ];

            if ( opcode == LL_CHANNEL_MAP_REQ )
            {
                channels_.reset( &body[ 1 ] );
            }
            else if ( opcode == LL_CONNECTION_UPDATE_REQ )
            {
                if ( parse_timing_parameters_from_connection_update_request( body ) )
                {
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
            connection_interval_.usec() / us_per_digits,
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
    std::uint64_t link_layer< Server, ScheduledRadio, Options... >::read_64( const std::uint8_t* p )
    {
        return static_cast< std::uint64_t >( read_32( p ) ) | static_cast< std::uint64_t >( read_32( p + 4 ) ) << 32;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::size_t link_layer< Server, ScheduledRadio, Options... >::fill_l2cap_advertising_data( std::uint8_t* buffer, std::size_t buffer_size ) const
    {
        return server_->advertising_data( buffer, buffer_size );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::size_t link_layer< Server, ScheduledRadio, Options... >::fill_l2cap_scan_response_data( std::uint8_t* buffer, std::size_t buffer_size ) const
    {
        return server_->scan_response_data( buffer, buffer_size );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    const device_address& link_layer< Server, ScheduledRadio, Options... >::local_address() const
    {
        return address_;
    }

}
}

#endif
